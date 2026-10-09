"""Actual native e1000/UDP authentication, ring wrap and reboot/reconnect in q35.

Raw Ethernet is confined to QEMU's loopback TCP backend. No LAN, TAP, Dell
boot changes, public listeners or production credentials are involved.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import time
from network_wire import MAC,PEER_MAC,IP,PEER_IP,arp,ping,udp,request,verify_response,parse_ipv4,checksum,ethernet,ipv4

repo=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=repo/'build/native-network');p.add_argument('--transport',choices=['e1000','usb_ecm'],default='e1000');a=p.parse_args()
build=a.build.resolve();manifest=json.loads((build/'manifest.json').read_text());assert manifest['vm_only']
usb=a.transport=='usb_ecm';roundtrips=140 if usb else 80
assert (manifest.get('vm_usb_network') and manifest['vm_usb'] and not manifest['vm_network']) if usb else manifest['vm_network']
out=build/('vm-usb-network' if usb else 'vm-network');out.mkdir(parents=True,exist_ok=True);runtime=repo/'build/tools/qemu'
shutil.copyfile(runtime/'share/edk2-i386-vars.fd',out/'vars.fd');shutil.copytree(build/'esp',out/'esp',dirs_exist_ok=True)
def free_port():
    with socket.socket() as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]
port=free_port();qmp_port=free_port();serial=out/'serial.log';serial.write_text('');log=(out/'qemu.log').open('w')
device=['-device','qemu-xhci,id=xhci','-device','usb-net,bus=xhci.0,port=1,netdev=native,mac=52:54:00:12:34:56'] if usb else ['-device','e1000,netdev=native,mac=52:54:00:12:34:56']
start=time.monotonic();process=subprocess.Popen([str(runtime/'qemu-system-x86_64.exe'),'-machine','q35','-cpu','max,phys-bits=36' if usb else 'max','-accel','tcg','-m','256M','-display','none',
    '-drive',f'if=pflash,format=raw,readonly=on,file={runtime/"share/edk2-x86_64-code.fd"}',
    '-drive',f'if=pflash,format=raw,file={out/"vars.fd"}',
    '-drive',f'format=raw,file=fat:rw:{out/"esp"}',
    '-netdev',f'socket,id=native,listen=127.0.0.1:{port}',*device,
    '-qmp',f'tcp:127.0.0.1:{qmp_port},server=on,wait=off','-serial',f'file:{serial}'],stdout=log,stderr=log,creationflags=0x08000000)
wire=None;qmp=None;wire_buffer=bytearray()
def alive():assert process.poll() is None,(out/'qemu.log').read_text()
def connect():
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        alive()
        try:
            s=socket.create_connection(('127.0.0.1',port),timeout=1);s.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1);s.settimeout(2);return s
        except OSError:time.sleep(.1)
    raise RuntimeError('QEMU network listener unavailable')
def wait_ready(count):
    deadline=time.monotonic()+40
    while time.monotonic()<deadline:
        alive();text=serial.read_text(errors='replace')
        if text.count('KERNEL_NETWORK_READY')>=count:return text
        if any(marker in text for marker in ['KERNEL_NETWORK_DRIVER_FAILED','KERNEL_NETWORK_NONCE_UNAVAILABLE','KERNEL_USB_INSPECTION_FAILED','KERNEL_USB_NETWORK_FAILED']):raise RuntimeError(text[-3000:])
        time.sleep(.1)
    raise RuntimeError('Native network startup absent\n'+text[-3000:])
def send(frame):wire.sendall(struct.pack('>I',len(frame))+frame)
def receive():
    # Preserve partial TCP records across observation timeouts.
    while True:
        if len(wire_buffer)>=4:
            length=struct.unpack_from('>I',wire_buffer)[0];assert 14<=length<=65536
            if len(wire_buffer)>=4+length:
                frame=bytes(wire_buffer[4:4+length]);del wire_buffer[:4+length];return frame
        block=wire.recv(65536);assert block,'QEMU network socket closed';wire_buffer.extend(block)
def exchange(frame,predicate):
    # Send each request once. Retrying HELLO while TCG is delayed creates
    # additional legitimate replies that would contaminate later silence tests.
    send(frame);deadline=time.monotonic()+5
    while time.monotonic()<deadline:
        wire.settimeout(max(.01,deadline-time.monotonic()))
        try:
            response=receive()
            if predicate(response):wire.settimeout(2);return response
        except socket.timeout:break
    raise AssertionError('No matching native packet reply\n'+serial.read_text(errors='replace')[-3000:])
def silent(frame):
    send(frame);wire.settimeout(.15)
    try:reply=receive();raise AssertionError('Rejected packet received a reply: '+reply.hex())
    except socket.timeout:pass
    finally:wire.settimeout(2)
def is_udp(frame):return len(frame)>=42 and frame[:6]==PEER_MAC and frame[12:14]==b'\x08\0' and frame[23]==17
def command(operation,sequence,nonce,challenge):
    frame=exchange(udp(request(operation,sequence,nonce,challenge)),is_udp)
    return verify_response(frame,operation,sequence,challenge,None if operation==1 else nonce)
try:
    wire=connect();wait_ready(1)
    arp_reply=exchange(arp(),lambda f:len(f)>=42 and f[12:14]==b'\x08\x06' and f[20:22]==b'\0\2')
    assert arp_reply[:6]==PEER_MAC and arp_reply[6:12]==MAC and arp_reply[22:28]==MAC and arp_reply[28:32]==IP and arp_reply[38:42]==PEER_IP
    challenge=os.urandom(16);nonce,hello=command(1,0,b'\0'*16,challenge);assert hello[-1]==0
    _,first=command(2,1,nonce,challenge);assert first[1]>0 and first[-1]==1
    invalid=[]
    bad=bytearray(request(3,2,nonce,challenge));bad[-1]^=1;invalid.append(('bad_hmac_reboot',udp(bytes(bad))))
    invalid.append(('replay_status',udp(request(2,1,nonce,challenge))))
    invalid.append(('wrong_boot_nonce',udp(request(3,2,b'\x55'*16,challenge))))
    invalid.append(('unknown_signed_command',udp(request(9,2,nonce,challenge))))
    invalid.append(('wrong_port',udp(request(3,2,nonce,challenge),destination_port=47334)))
    valid=udp(request(3,2,nonce,challenge));bad=bytearray(valid);bad[24]^=1;invalid.append(('bad_ip_checksum',bytes(bad)))
    bad=bytearray(valid);bad[40]^=1;invalid.append(('bad_udp_checksum',bytes(bad)))
    invalid.append(('fragment',ethernet(0x800,ipv4(17,valid[34:],flags=0x2000))))
    invalid.append(('truncated',valid[:-1]))
    invalid.append(('udp_checksum_omitted',valid[:40]+b'\0\0'+valid[42:]))
    if usb:
        # QEMU usb-net has a 2048-byte input buffer. Larger frames never
        # reach our driver; these fixtures exercise native rejection instead.
        invalid.append(('oversized_usb_short_frame',b'\xff'*1536))
        tail=udp(request(3,2,nonce,challenge))
        invalid.append(('oversized_usb_full_frame_with_signed_tail',b'\xff'*(2048-len(tail))+tail))
    for label,frame in invalid:
        try:silent(frame)
        except Exception as error:raise AssertionError(label) from error
    assert serial.read_text(errors='replace').count('KERNEL_NETWORK_READY')==1
    if usb:assert 'KERNEL_USB_OVERSIZE_FRAME_DROPPED' in serial.read_text(errors='replace')
    # More than two full 32-entry DMA ring rotations in both directions.
    for sequence in range(roundtrips):
        length=(1400 if sequence==roundtrips-1 else 15)+(sequence%2)
        if usb and sequence%3==0:length=22+64*(sequence%8) # whole frame is a multiple of bulk MPS: requires ZLP
        payload=bytes([sequence])*length
        try:response=exchange(ping(sequence,payload),lambda f:len(f)>=42 and f[23]==1)
        except Exception as error:raise AssertionError('Packet roundtrip '+str(sequence)+' length '+str(length)) from error
        protocol,icmp=parse_ipv4(response)
        assert protocol==1 and icmp[0]==0 and checksum(icmp)==0 and struct.unpack_from('>H',icmp,6)[0]==sequence and icmp[8:]==payload
    _,second=command(2,2,nonce,challenge);assert second[0]>first[0] and second[2]>=roundtrips+10 and second[3]>=roundtrips and second[-1]==2
    old_status=udp(request(2,2,nonce,challenge));old_reboot=udp(request(3,3,nonce,challenge))
    command(3,3,nonce,challenge)  # The only reset request; no QMP reset is used.
    wire.close();wire=None;wire_buffer.clear();wire=connect();text=wait_ready(2)
    assert 'KERNEL_AUTHENTICATED_REBOOT reset_vm' in text and text.count('LOADER_ENTER')>=2
    challenge2=os.urandom(16);nonce2,hello2=command(1,0,b'\0'*16,challenge2)
    assert nonce2!=nonce and hello2[-1]==0
    silent(old_status);silent(old_reboot)
    _,fresh=command(2,1,nonce2,challenge2);assert fresh[-1]==1 and fresh[1]>0
    # Freeze the guest only after all network/reboot behavior has been observed.
    qmp=socket.create_connection(('127.0.0.1',qmp_port),timeout=3);stream=qmp.makefile('rwb');assert 'QMP' in json.loads(stream.readline())
    def qcommand(name):
        stream.write(json.dumps({'execute':name}).encode()+b'\n');stream.flush()
        while True:
            reply=json.loads(stream.readline())
            if 'error' in reply:raise RuntimeError(reply)
            if 'return' in reply:return reply['return']
    qcommand('qmp_capabilities');qcommand('stop')
    result={'passed':True,'transport':a.transport,'arp_icmp_authenticated_udp':True,'dma_ring_rotations':2,'ring_roundtrips':roundtrips,
            'usb_bulk_zlp_boundaries_verified':usb,
            'invalid_packets_rejected':[label for label,_ in invalid],'authenticated_guest_reboot':True,
            'reconnected_with_fresh_boot_nonce':True,'old_boot_commands_rejected':True,'native_boots':2,
            'seconds':round(time.monotonic()-start,3),'manifest':manifest,'serial_sha256':hashlib.sha256(serial.read_bytes()).hexdigest(),
            'scope':'isolated q35 '+a.transport+' VM fixture; public test key; no LAN/physical native control claim'}
    (out/'verification.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
finally:
    if wire:wire.close()
    if qmp:qmp.close()
    if process.poll() is None:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait()
    log.close()

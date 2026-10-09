"""Exercise Companion-owned xHCI rings/descriptors in isolated QEMU."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

repo=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=repo/'build/native-usb')
p.add_argument('--case',choices=['devices','empty','absent','highbar'],default='devices')
a=p.parse_args();build=a.build.resolve();manifest=json.loads((build/'manifest.json').read_text())
assert manifest['vm_only'] and manifest['vm_usb'] and not manifest['vm_network']
out=build/('vm-usb-'+a.case);out.mkdir(parents=True,exist_ok=True)
runtime=repo/'build/tools/qemu'
shutil.copyfile(runtime/'share/edk2-i386-vars.fd',out/'vars.fd')
shutil.copytree(build/'esp',out/'esp',dirs_exist_ok=True)
with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
args=[str(runtime/'qemu-system-x86_64.exe'),'-machine','q35','-cpu','qemu64,phys-bits='+('40' if a.case=='highbar' else '36'),'-accel','tcg','-m','256M','-display','none','-net','none','-no-reboot',
      '-drive',f'if=pflash,format=raw,readonly=on,file={runtime/"share/edk2-x86_64-code.fd"}',
      '-drive',f'if=pflash,format=raw,file={out/"vars.fd"}',
      '-drive',f'format=raw,file=fat:rw:{out/"esp"}',
      '-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off','-serial',f'file:{out/"serial.log"}']
if a.case!='absent':args+=['-device','qemu-xhci,id=xhci']
if a.case=='devices':
    (out/'usb.img').write_bytes(bytes(1024*1024))
    args+=['-device','usb-kbd,bus=xhci.0,port=1',
           '-netdev','user,id=usbnet,restrict=on','-device','usb-net,bus=xhci.0,port=2,netdev=usbnet',
           '-drive',f'if=none,id=usbfixture,format=raw,file={out/"usb.img"}',
           '-device','usb-storage,bus=xhci.0,port=3,drive=usbfixture']
serial=out/'serial.log';serial.write_text('');log=(out/'qemu.log').open('w')
process=subprocess.Popen(args,stdout=log,stderr=log,creationflags=0x08000000)
connection=None;start=time.monotonic()
try:
    deadline=start+60
    while time.monotonic()<deadline:
        assert process.poll() is None,(out/'qemu.log').read_text()
        text=serial.read_text(errors='replace')
        if 'KERNEL_PROGRESS 0x0000000000000001' in text:break
        time.sleep(.2)
    else:raise RuntimeError('Native USB evidence missing\n'+text[-3000:])
    assert 'KERNEL_ENTER firmware_services=off' in text and 'KERNEL_EXCEPTION vector=' not in text
    observed=[]
    for line in text.splitlines():
        if line.startswith('USB_DEVICE '):observed.append({k:int(v,16) for k,v in re.findall(r'(\w+)=(0x[0-9A-Fa-f]+)',line)})
    if a.case=='absent':assert 'KERNEL_USB_ADAPTER_ABSENT' in text and not observed
    elif a.case=='highbar':assert 'KERNEL_USB_MAPPING_REJECTED' in text and 'KERNEL_USB_READY' not in text and not observed
    else:
        assert 'KERNEL_USB_READY xhci vm_fixture_only' in text and 'KERNEL_USB_INSPECTION_OK' in text,text[-3500:]
        summary=next(line for line in text.splitlines() if line.startswith('KERNEL_USB_INSPECTION_OK'))
        stats={k:int(v,16) for k,v in re.findall(r'(\w+)=(0x[0-9A-Fa-f]+)',summary)}
        assert stats['commands']>=160 and stats['events']>=160 and stats['pages_restored']==1
        assert stats['devices']==len(observed)
        if a.case=='empty':assert not observed and stats['transfers']==0
        else:
            assert len(observed)==3,observed
            assert {(d['vendor'],d['product']) for d in observed}=={(0x627,1),(0x525,0xa4a2),(0x46f4,1)},observed
            assert {d['speed'] for d in observed}=={1,3,4},observed
            assert {(d['vendor'],d['product']):(d['speed'],d['ep0_packet'],d['configuration_bytes'],d['interfaces']) for d in observed}=={
                (0x627,1):(3,64,34,1),(0x525,0xa4a2):(1,64,67,2),(0x46f4,1):(4,512,44,1)},observed
            assert all(d['configuration_bytes']>=18 and d['interfaces'] for d in observed)
            assert stats['transfers']==12,stats
    connection=socket.create_connection(('127.0.0.1',port),timeout=3);stream=connection.makefile('rwb');assert 'QMP' in json.loads(stream.readline())
    def command(name,arguments=None):
        request={'execute':name}
        if arguments:request['arguments']=arguments
        stream.write(json.dumps(request).encode()+b'\n');stream.flush()
        while True:
            reply=json.loads(stream.readline())
            if 'error' in reply:raise RuntimeError(reply)
            if 'return' in reply:return reply['return']
    command('qmp_capabilities');usb=command('human-monitor-command',{'command-line':'info usb'})
    (out/'qmp-usb.txt').write_text(usb)
    if a.case=='devices':assert all(name in usb for name in ['QEMU USB Keyboard','QEMU USB Network Interface','QEMU USB MSD']),usb
    command('stop');time.sleep(.1)
    report={'passed':True,'case':a.case,'native_devices':observed,'ring_wrap_commands':160 if a.case in ['devices','empty'] else 0,
            'dma_pages_restored':a.case not in ['absent','highbar'],'duration_seconds':round(time.monotonic()-start,3),
            'kernel_sha256':hashlib.sha256((build/'kernel.elf').read_bytes()).hexdigest(),
            'loader_sha256':hashlib.sha256((build/'companion-loader.efi').read_bytes()).hexdigest(),
            'trace_sha256':hashlib.sha256(serial.read_bytes()).hexdigest(),
            'scope':'QEMU root-port control transfers only; no physical USB/Ethernet or hub support'}
    (out/'verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
finally:
    if connection:connection.close()
    process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    log.close()

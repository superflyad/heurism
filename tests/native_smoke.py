"""Boot Companion's own EFI loader and ELF kernel in isolated QEMU/OVMF."""
import argparse
import ctypes
import hashlib
import json
import re
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import time
import zlib

repo=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=repo/'build/native');p.add_argument('--reject',choices=['missing','corrupt']);a=p.parse_args()
build=a.build.resolve();manifest=json.loads((build/'manifest.json').read_text());assert manifest['vm_only'],'COM1 diagnostics must be explicitly enabled'
fault=manifest.get('exception_test') or manifest.get('pagefault_test')
name=a.reject or ('pagefault' if manifest.get('pagefault_test') else 'exception' if manifest.get('exception_test') else 'stale-map' if manifest['stale_map_test'] else 'normal');out=build/('vm-'+name);out.mkdir(parents=True,exist_ok=True)
runtime=repo/'build/tools/qemu';shutil.copyfile(runtime/'share/edk2-i386-vars.fd',out/'vars.fd');shutil.copytree(build/'esp',out/'esp',dirs_exist_ok=True)
kernel=out/'esp/EFI/Companion/kernel.elf'
if a.reject=='missing':kernel.unlink()
if a.reject=='corrupt':data=bytearray(kernel.read_bytes());data[:4]=b'FAIL';kernel.write_bytes(data)
class Framebuffer(ctypes.Structure):
    _fields_=[('pixels',ctypes.POINTER(ctypes.c_uint32)),('width',ctypes.c_uint32),('height',ctypes.c_uint32),('stride',ctypes.c_uint32),('format',ctypes.c_uint32),('size',ctypes.c_uint64)]
dll=ctypes.CDLL(str(build/'native-host.dll'));dll.framebuffer_text.argtypes=[ctypes.POINTER(Framebuffer),ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_char_p,ctypes.c_uint32]
def check_frame(path,input_text=None):
    with path.open('rb') as f:
        assert f.readline().strip()==b'P6';w,h=map(int,f.readline().split());assert f.readline().strip()==b'255';data=f.read()
    pixels=(ctypes.c_uint32*(w*h))();fb=Framebuffer(pixels,w,h,w,1,w*h*4);scale=5 if w>=960 and h>=600 else 2
    x=max(0,(w-54*scale)//2);y=h//3;dll.framebuffer_text(ctypes.byref(fb),x,y,scale,b'COMPANION',0xf0f5f3)
    for row in range(y,y+7*scale):
        for col in range(x,x+54*scale):
            px=pixels[row*w+col];expected=bytes(((px>>16)&255,(px>>8)&255,px&255)) if px else bytes.fromhex('101820')
            assert data[(row*w+col)*3:(row*w+col+1)*3]==expected,(row,col)
    if input_text is not None:
        iy=y+10*scale+110
        for i in range(w*h):pixels[i]=0
        dll.framebuffer_text(ctypes.byref(fb),x,iy,2,input_text.encode(),0xf0f5f3)
        for row in range(iy,iy+14):
            for col in range(x,x+len(input_text)*12):
                px=pixels[row*w+col];expected=bytes(((px>>16)&255,(px>>8)&255,px&255)) if px else bytes.fromhex('101820')
                assert data[(row*w+col)*3:(row*w+col+1)*3]==expected,('input',row,col)
        for i,c in enumerate(input_text):
            if c!=' ':
                assert any(data[(row*w+col)*3:(row*w+col+1)*3]!=bytes.fromhex('101820')
                           for row in range(iy,iy+14) for col in range(x+i*12,x+i*12+10)),('invisible input glyph',c)
    rows=b''.join(b'\0'+data[i*w*3:(i+1)*w*3] for i in range(h))
    def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d))
    path.with_suffix('.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
log=(out/'qemu.log').open('w');serial=out/'serial.log';serial.write_text('')
start=time.monotonic()
process=subprocess.Popen([str(runtime/'qemu-system-x86_64.exe'),'-machine','q35','-accel','tcg','-m','256M','-display','none','-net','none','-no-reboot',
    '-drive',f'if=pflash,format=raw,readonly=on,file={runtime/"share/edk2-x86_64-code.fd"}',
    '-drive',f'if=pflash,format=raw,file={out/"vars.fd"}',
    '-drive',f'format=raw,file=fat:rw:{out/"esp"}',
    '-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off','-serial',f'file:{serial}'],stdout=log,stderr=log,creationflags=0x08000000)
connection=None
pci_verified=False;input_verified=False;boot_marker_seconds=None
try:
    deadline=time.monotonic()+60
    while time.monotonic()<deadline:
        assert process.poll() is None,(out/'qemu.log').read_text()
        text=serial.read_text(errors='replace')
        if a.reject and 'LOADER_REJECTED' in text:break
        if manifest.get('exception_test') and 'KERNEL_EXCEPTION vector=0x0000000000000006' in text:break
        if manifest.get('pagefault_test') and 'KERNEL_PAGEFAULT address=0x0000000002000000' in text:break
        if not a.reject and 'KERNEL_PROGRESS 0x0000000000000001' in text:break
        time.sleep(.25)
    else:raise RuntimeError('Boot evidence missing: '+str(serial)+'\n'+text[-2500:])
    boot_marker_seconds=round(time.monotonic()-start,3)
    if a.reject:assert 'EXIT_BOOT_SERVICES_BEGIN' not in text and 'KERNEL_ENTER' not in text
    else:
        assert 'ELF_VALIDATED' in text and 'KERNEL_ENTER firmware_services=off' in text and 'KERNEL_FRAMEBUFFER_READY' in text
        assert 'KERNEL_EXCEPTIONS_READY' in text and 'KERNEL_PAGES_READY' in text
        assert 'KERNEL_PAGE_TABLES_READY' in text
        assert 'KERNEL_ACPI_READY' in text and 'ACPI_TABLE APIC' in text and 'ACPI_TABLE MCFG' in text
        assert 'KERNEL_PCI_READY' in text
        if fault:assert 'KERNEL_PROGRESS' not in text
        else:assert 'KERNEL_TIMER_READY' in text
        wanted='2' if manifest['stale_map_test'] else '1';assert 'EXIT_BOOT_SERVICES_OK attempts=0x000000000000000'+wanted in text
        connection=socket.create_connection(('127.0.0.1',port),timeout=3);stream=connection.makefile('rwb');assert 'QMP' in json.loads(stream.readline())
        def command(name,arguments=None):
            req={'execute':name};req.update({'arguments':arguments} if arguments else {});stream.write(json.dumps(req).encode()+b'\n');stream.flush()
            while True:
                reply=json.loads(stream.readline())
                if 'error' in reply:raise RuntimeError(reply)
                if 'return' in reply:return reply['return']
        command('qmp_capabilities')
        pci=command('query-pci');(out/'qmp-pci.json').write_text(json.dumps(pci,indent=2))
        expected=set()
        def collect(bus):
            for d in bus['devices']:
                expected.add((d['bus'],d['slot'],d['function'],d['id']['vendor'],d['id']['device'],d['class_info']['class']))
                if 'pci_bridge' in d:collect(d['pci_bridge']['bus'])
        for bus in pci:collect(bus)
        observed=set()
        for line in text.splitlines():
            if line.startswith('PCI_DEVICE '):
                fields={k:int(v,16) for k,v in re.findall(r'(\w+)=(0x[0-9A-Fa-f]+)',line)}
                assert fields['segment']==0
                observed.add(tuple(fields[k] for k in ['bus','slot','function','vendor','device'])+(fields['class']*256+fields['subclass'],))
        assert observed==expected,(observed,expected)
        pci_verified=True
        if not fault and manifest.get('keyboard')=='i8042':
            assert 'KERNEL_INPUT_READY i8042=translated-set1' in text
            def send(keys,wanted):
                previous=len(serial.read_text(errors='replace'))
                command('send-key',{'keys':[{'type':'qcode','data':key} for key in keys],'hold-time':60})
                codes={'a':0x1e,'shift':0x2a,'b':0x30,'backspace':0xe,'c':0x2e,'caps_lock':0x3a,'d':0x20,'esc':1}
                releases={codes[key] for key in keys}
                end=time.monotonic()+3
                while time.monotonic()<end:
                    new=serial.read_text(errors='replace')[previous:]
                    seen={int(code,16) for code in re.findall(r'KEY_EVENT code=(0x[0-9A-F]+) pressed=0x0000000000000000',new)}
                    if wanted in new and releases<=seen:return
                    time.sleep(.02)
                raise AssertionError('Native input missing: '+wanted+'\n'+new)
            send(['a'],'KERNEL_INPUT_TEXT a\n')
            send(['shift','b'],'KERNEL_INPUT_TEXT aB\n')
            send(['backspace'],'KERNEL_INPUT_TEXT a\n')
            send(['c'],'KERNEL_INPUT_TEXT ac\n')
            send(['caps_lock'],'KEY_EVENT code=0x000000000000003A')
            send(['d'],'KERNEL_INPUT_TEXT acD\n')
            command('screendump',{'filename':str(out/'input.ppm')});check_frame(out/'input.ppm','acD')
            send(['esc'],'KERNEL_INPUT_TEXT \n')
            input_verified=True
        elif not fault:assert 'KERNEL_INPUT_READY' not in text
        command('stop')
        command('screendump',{'filename':str(out/'companion.ppm')});check_frame(out/'companion.ppm')
    result={'case':name,'passed':True,'boot_marker_seconds':boot_marker_seconds,'pci_matches_qmp':pci_verified,'native_keyboard_verified':input_verified,'seconds_from_qemu_launch_to_evidence':round(time.monotonic()-start,3),'manifest':manifest,'serial_sha256':hashlib.sha256(serial.read_bytes()).hexdigest(),'scope':'VM only; wall time includes OVMF/TCG startup, not a physical boot measurement'}
    (out/'verification.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
finally:
    if connection:connection.close()
    if process.poll() is None:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait()
    log.close()

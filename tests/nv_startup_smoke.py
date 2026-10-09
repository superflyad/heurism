"""Execute the real startup EFI driver in isolated OVMF; no physical target."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--clang',required=True);parser.add_argument('--linker',required=True)
args=parser.parse_args();repo=Path(__file__).resolve().parents[1];build=repo/'build/nv-startup'
driver=(build/'companionextx64.efi').read_bytes()
(build/'driver.h').write_text('static const U8 vm_driver[]={'+','.join(str(b) for b in driver)+'};')
qemu=repo/'build/tools/qemu/qemu-system-x86_64.exe'
firmware=repo/'build/network-root/ovmf'
reports=[]
for case,name in enumerate(['valid-nv','missing-nv','corrupt-nv']):
 out=build/('vm-'+name);esp=out/'esp/EFI/BOOT';esp.mkdir(parents=True,exist_ok=True)
 flags=['--target=x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fshort-wchar','-mno-red-zone','-fno-stack-protector','-fno-builtin','-Wall','-Wextra','-Werror','-O2',f'-DSTARTUP_VM_CASE={case}']
 subprocess.run([args.clang,*flags,'-c',str(repo/'tests/nv_startup_vm.c'),'-o',str(out/'app.obj')],check=True)
 subprocess.run([args.linker,'/subsystem:efi_application','/entry:efi_main','/nodefaultlib','/machine:x64','/dynamicbase','/timestamp:0','/out:'+str(esp/'BOOTX64.EFI'),str(out/'app.obj')],check=True)
 shutil.copyfile(firmware/'OVMF_VARS.fd',out/'vars.fd')
 command=[str(qemu),'-machine','q35','-accel','tcg','-m','256M','-display','none','-net','none',
  '-drive','if=pflash,format=raw,readonly=on,file='+str(firmware/'OVMF_CODE.fd'),
  '-drive','if=pflash,format=raw,file='+str(out/'vars.fd'),
  '-drive','format=raw,file=fat:rw:'+str(out/'esp'),
  '-debugcon','file:'+str(out/'debug.log'),'-global','isa-debugcon.iobase=0xe9',
  '-device','isa-debug-exit,iobase=0xf4,iosize=4']
 started=time.monotonic()
 with (out/'qemu.log').open('w') as log:
  process=subprocess.Popen(command,stdout=log,stderr=log,creationflags=0x08000000)
  try:code=process.wait(timeout=50)
  finally:
   if process.poll() is None:
    process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait()
 debug=(out/'debug.log').read_text()
 assert code==33 and 'NV_STARTUP_VM_PASS' in debug,(name,code,debug,(out/'qemu.log').read_text())
 reports.append({'case':name,'passed':True,'seconds':round(time.monotonic()-started,2)})
 print('PASS: '+name,flush=True)
report={'driver_sha256':hashlib.sha256(driver).hexdigest(),'cases':reports,
 'scope':'OVMF LoadImage/StartImage on actual driver, VM-only NV fixtures, volatile marker and owner service. Does not test physical DriverOrder or firmware fallback.'}
(build/'vm-verification.json').write_text(json.dumps(report,indent=2))

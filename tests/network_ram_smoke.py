"""Boot the RAM image with no disk device and verify pinned root SSH."""
from pathlib import Path
import argparse
import json
import socket
import subprocess
import time
import shutil
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--uefi-pxe',action='store_true')
parser.add_argument('--exercise-recovery',action='store_true')
args=parser.parse_args()
repo=Path(__file__).resolve().parents[1];out=repo/'build/network-root/test';out.mkdir(parents=True,exist_ok=True)
qemu=repo/'build/tools/qemu/qemu-system-x86_64.exe';keys=repo/'artifacts/ssh'
with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
ssh=['ssh','-i',str(keys/'companion_client_ed25519'),'-p',str(port),'-o','BatchMode=yes',
 '-o','ConnectTimeout=2','-o','StrictHostKeyChecking=yes','-o','HostKeyAlias=companion-dell',
 '-o','UserKnownHostsFile='+str(keys/'known_hosts'),'root@127.0.0.1']
log=(out/'qemu.log').open('w')
base=[str(qemu),'-machine','q35','-accel','tcg','-m','2048M','-smp','2','-display','none']
if args.uefi_pxe:
 shutil.copyfile(repo/'build/network-root/ovmf/OVMF_VARS.fd',out/'vars.fd')
 boot=['-drive','if=pflash,format=raw,readonly=on,file='+str(repo/'build/network-root/ovmf/OVMF_CODE.fd'),
  '-drive','if=pflash,format=raw,file='+str(out/'vars.fd'),'-boot','order=n,strict=on']
 network=f'user,id=net,tftp={repo / "build/network-root"},bootfile=companion-netboot.efi,hostfwd=tcp:127.0.0.1:{port}-:22'
else:
 boot=[
 '-kernel',str(repo/'build/network-root/vmlinuz'),'-initrd',str(repo/'build/network-root/initramfs'),
 '-append','rdinit=/init console=ttyS0,115200n8 panic=15 module_blacklist=nvme,vmd,ahci,sd_mod,usb_storage companion_network_boot=1']
 network=f'user,id=net,hostfwd=tcp:127.0.0.1:{port}-:22'
process=subprocess.Popen(base+boot+[
 '-netdev',network,'-device',('virtio-net-pci,netdev=net,bootindex=1,romfile=' if args.uefi_pxe else 'e1000,netdev=net,bootindex=1'),
 '-serial','file:'+str(out/'serial.log')],stdout=log,stderr=log,creationflags=0x08000000)
try:
 deadline=time.monotonic()+180
 while time.monotonic()<deadline:
  assert process.poll() is None,(out/'qemu.log').read_text()
  r=subprocess.run(ssh+['test "$(id -u)" = 0 && test -f /etc/companion/network-root && cat /proc/sys/kernel/random/boot_id'],capture_output=True,text=True,timeout=5,creationflags=0x08000000)
  if r.returncode==0:break
  time.sleep(2)
 else:raise RuntimeError('RAM root SSH failed; inspect '+str(out/'serial.log'))
 command='set -eu; findmnt /; cat /proc/cmdline; test -z "$(findmnt -rn -o SOURCE | grep ^/dev/ || true)"; test -z "$(ls /sys/class/block | grep -E '+"'^(sd|vd|nvme)'"+')"; sshd -t; ip -4 -o addr; cat /etc/companion/network-root'
 proof=subprocess.run(ssh+[command],capture_output=True,text=True,timeout=10,creationflags=0x08000000)
 assert proof.returncode==0,proof.stdout+proof.stderr
 recovery=False
 if args.exercise_recovery:
  fault=subprocess.run(ssh+['kill "$(cat /run/sshd.pid)"; for p in /run/companion/dhcp-*.pid; do kill "$(cat "$p")"; done'],capture_output=True,text=True,timeout=10,creationflags=0x08000000)
  assert fault.returncode==0,fault.stdout+fault.stderr
  until=time.monotonic()+45
  while time.monotonic()<until:
   check=subprocess.run(ssh+['test -f /run/companion/network-healthy && kill -0 "$(cat /run/sshd.pid)" && for p in /run/companion/dhcp-*.pid; do kill -0 "$(cat "$p")" || exit; done; cat /proc/sys/kernel/random/boot_id'],capture_output=True,text=True,timeout=5,creationflags=0x08000000)
   if check.returncode==0 and check.stdout.strip()==r.stdout.strip():recovery=True;break
   time.sleep(2)
  assert recovery,'RAM SSH/DHCP recovery failed'
 kind='pxe' if args.uefi_pxe else 'direct'
 if args.uefi_pxe:assert 'COMPANION_NETWORK_RAM_01' in (out/'serial.log').read_text(errors='replace')
 (out/(kind+'-proof.txt')).write_text(proof.stdout)
 (out/(kind+'-proof.json')).write_text(json.dumps({'boot_id':r.stdout.strip(),'no_disk_device_configured':True,
  'firmware_pxe_boot':args.uefi_pxe,
  'ssh_and_dhcp_crash_recovery':recovery,
  'pinned_root_ssh':True,'kernel_and_initramfs_manifest':json.loads((repo/'build/network-root/manifest.json').read_text())['hashes']},indent=2))
 print('PASS: '+kind+' boot, RAM root, no disk device, pinned root SSH; '+r.stdout.strip(),flush=True)
finally:
 process.terminate()
 try:process.wait(timeout=10)
 except subprocess.TimeoutExpired:process.kill();process.wait()
 log.close()

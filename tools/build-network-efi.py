"""Package the tested RAM kernel/initramfs into a standalone UEFI GRUB image."""
import hashlib
import json
from pathlib import Path
import subprocess
repo=Path(__file__).resolve().parents[1];out=repo/'build/network-root';keys=repo/'artifacts/ssh'
state=json.loads((repo/'artifacts/targets/dell.json').read_text())
options=['-i',str(keys/'companion_client_ed25519'),'-o','BatchMode=yes','-o','StrictHostKeyChecking=yes',
 '-o','HostKeyAlias=companion-dell','-o','UserKnownHostsFile='+str(keys/'known_hosts')]
target='root@'+state['address'];manifest=json.loads((out/'manifest.json').read_text())
for name in ['vmlinuz','initramfs']:assert hashlib.sha256((out/name).read_bytes()).hexdigest()==manifest['hashes'][name]
cfg='''set timeout=0
echo COMPANION_NETWORK_RAM_01
linux (memdisk)/boot/vmlinuz rdinit=/init console=ttyS0,115200n8 console=tty1 panic=15 module_blacklist=nvme,vmd,ahci,sd_mod,usb_storage companion_network_boot=1
initrd (memdisk)/boot/initramfs
boot
exit
'''
(out/'embedded.cfg').write_text(cfg,newline='\n')
for name in ['vmlinuz','initramfs','embedded.cfg']:
 subprocess.run(['scp']+options+[str(out/name),target+':/var/lib/companion/network-root-build/'+name],check=True,timeout=45)
command='''set -eu
cd /var/lib/companion/network-root-build
grub-mkstandalone -O x86_64-efi --locales='' --fonts='' --install-modules='normal linux memdisk tar gzio echo boot' --modules='normal linux memdisk tar gzio' -o companion-netboot.efi 'boot/grub/grub.cfg=embedded.cfg' 'boot/vmlinuz=vmlinuz' 'boot/initramfs=initramfs'
sha256sum companion-netboot.efi
'''
result=subprocess.run(['ssh']+options+[target,command],capture_output=True,text=True,timeout=45)
if result.returncode:raise SystemExit(result.stderr or result.stdout)
subprocess.run(['scp']+options+[target+':/var/lib/companion/network-root-build/companion-netboot.efi',str(out/'companion-netboot.efi')],check=True,timeout=45)
digest=hashlib.sha256((out/'companion-netboot.efi').read_bytes()).hexdigest();assert digest in result.stdout
report={'efi_sha256':digest,'efi_bytes':(out/'companion-netboot.efi').stat().st_size,
 'embedded_kernel_sha256':manifest['hashes']['vmlinuz'],'embedded_initramfs_sha256':manifest['hashes']['initramfs'],
 'no_disk_search_or_boot_files':True}
(out/'efi-manifest.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

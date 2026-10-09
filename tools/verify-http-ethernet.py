"""Run/collect the staged SSD inspection, preserving management boot defaults."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['reboot','verify'])
args=parser.parse_args();repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware'
metadata_path=root/'http-ethernet-probe-deployment.json';metadata=json.loads(metadata_path.read_text())
entry=metadata['entry'];assert re.fullmatch(r'[0-9a-fA-F]{4}',entry)
digest=metadata['sha256'];assert re.fullmatch(r'[0-9a-f]{64}',digest)
def run(command):
 p=subprocess.run([sys.executable,str(repo/'tools/dell.py'),'--command',command,'--timeout','40'],capture_output=True,text=True,timeout=55)
 if p.returncode:raise RuntimeError(p.stderr+p.stdout)
 return p.stdout
def save():metadata_path.write_text(json.dumps(metadata,indent=2))
current=run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
 'test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1; '
 'test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"; '
 'rc-service sshd status >/dev/null; rc-service companion-watch status >/dev/null; '
 'cat /proc/sys/kernel/random/boot_id').strip()
boots=run('efibootmgr -v');drivers=run('efibootmgr --driver -v')
probe=[line for line in boots.splitlines() if line.startswith('Boot'+entry) and 'Companion HTTP Ethernet 02\t' in line]
assert len(probe)==1 and '\\EFI\\companion\\httpethernet02x64.efi' in probe[0]
assert 'DriverOrder: 0000,0001\n' in drivers and 'BootNext:' not in boots
files=run('set -eu; sha256sum /boot/efi/EFI/alpine/grubx64.efi /boot/efi/EFI/boot/bootx64.efi '
 '/boot/efi/EFI/companion/recoveryx64.efi /boot/efi/EFI/companion/companionextx64.efi '
 '/boot/efi/EFI/companion/httpethernet02x64.efi; cat /boot/efi/companion/recovery/grubenv')
assert files.count('5bc0e512af43def3ad39ce90b5084f1e56c73abc98d520e01466b7a7c7724efc')==3
assert '2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4' in files
assert digest in files and 'companion_pending=0' in files
if args.action=='reboot':
 assert not metadata['reboot_requested'] and current==metadata['previous_boot_id']
 assert 'BootOrder: 0005,0000\n' in boots
 metadata['reboot_requested']=True;save()
 run('set -eu; efibootmgr --bootnext '+entry+'; sync; '
     "nohup sh -c 'sleep 2; reboot' >/var/lib/companion/firmware/probe-stage/http-ethernet-reboot.log 2>&1 </dev/null &")
 print('SSD inspection scheduled once; boot '+entry+'; previous boot '+current)
else:
 assert metadata['reboot_requested'] and current!=metadata['previous_boot_id']
 current_entry=re.search(r'^BootCurrent: ([0-9A-Fa-f]{4})$',boots,re.M)[1]
 assert current_entry in [entry,'0005','0000']
 text=run('cat /boot/efi/EFI/companion/http-ethernet-probe-02.txt')
 (root/'http-ethernet-observation.txt').write_text(text)
 assert text.startswith('COMPANION_HTTP_ETHERNET_02\n') and text.endswith('PROBE_COMPLETE\n')
 assert 'HTTP_INITIALIZATION_COMPLETE\n' in text
 (root/'http-ethernet-boot-after.txt').write_text(boots)
 # Firmware can append auto-created NIC entries. Accept only the observed
 # matching USB NIC additions behind both preserved SSD entries before removal.
 order=re.search(r'^BootOrder: ([0-9A-Fa-f,]+)$',boots,re.M)[1].split(',')
 assert order[:2]==['0005','0000']
 for added in order[2:]:
  line=next(x for x in boots.splitlines() if x.startswith('Boot'+added))
  assert 'USB NIC (IPV' in line and 'MAC(7cc2c61db2f5,0)' in line and '{auto_created_boot_option}' in line
 run('set -eu; efibootmgr --bootnum '+entry+' --delete-bootnum; efibootmgr --bootorder 0005,0000; efibootmgr')
 metadata['verification']={'boot_id':current,'report_sha256':hashlib.sha256(text.encode()).hexdigest(),
  'healthy_root_returned':True,'temporary_entry_removed':True,'normal_order_restored':'0005,0000',
  'driver_order_preserved':'0000,0001','observed_boot_order':','.join(order),
  'boot_current_after_handoff':current_entry,
  'ipv4_supported': 'ETHERNET_IPV4_SUPPORTED status=0x0000000000000000' in text,
  'ipv6_supported': 'ETHERNET_IPV6_SUPPORTED status=0x0000000000000000' in text};save()
 print(json.dumps(metadata['verification'],indent=2))

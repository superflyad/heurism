"""Capture native network inventory and remove only its temporary boot entry."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware'
state=json.loads((repo/'artifacts/targets/dell.json').read_text())
deployment=json.loads((root/'network-probe-deployment.json').read_text())
identity=repo/'artifacts/ssh'
ssh=['ssh','-i',str(identity/'companion_client_ed25519'),'-o','BatchMode=yes',
 '-o','StrictHostKeyChecking=yes','-o','HostKeyAlias=companion-dell',
 '-o','UserKnownHostsFile='+str(identity/'known_hosts'),'root@'+state['address']]
def run(command):
 p=subprocess.run(ssh+[command],capture_output=True,timeout=30)
 if p.returncode:raise SystemExit(p.stderr.decode(errors='replace') or p.stdout.decode(errors='replace'))
 return p.stdout
boot_id=run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
 'test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"; '
 'rc-service sshd status >/dev/null; rc-service companion-watch status >/dev/null; '
 'cat /proc/sys/kernel/random/boot_id').decode().strip()
if boot_id==deployment['previous_boot_id']:raise SystemExit('Fresh boot required')
entry=deployment['entry']
if not re.fullmatch(r'[0-9A-Fa-f]{4}',entry):raise SystemExit('Invalid entry')
boot=run('efibootmgr -v').decode()
lines=[x for x in boot.splitlines() if x.startswith('Boot'+entry)]
if len(lines)!=1 or 'Companion Network Probe 01\t' not in lines[0] or '\\EFI\\companion\\networkprobex64.efi' not in lines[0]:
 raise SystemExit('Own entry differs')
data=run('cat /boot/efi/EFI/companion/network-probe-01.txt');text=data.decode()
(root/'network-probe-observation.txt').write_bytes(data)
complete=text.startswith('COMPANION_NETWORK_PROBE_01\n') and text.endswith('NETWORK_INVENTORY_COMPLETE\nPROBE_COMPLETE\n')
run('set -eu; efibootmgr --quiet --bootnum '+entry+' --delete-bootnum; efibootmgr --quiet --bootorder 0005,0000')
final=run('efibootmgr; efibootmgr --driver').decode()
assert 'BootNext:' not in final and 'BootOrder: 0005,0000\n' in final and 'DriverOrder: 0000,0001\n' in final
report={'boot_id':boot_id,'complete':complete,'report_sha256':hashlib.sha256(data).hexdigest(),
 'native_network_records':[x for x in text.splitlines() if x.startswith('NET_')],
 'root_and_watcher_healthy':True,'normal_boot_order_restored':True,'temporary_entry_removed':True,
 'network_transaction_performed':False,'ssd_independent_boot_proven':False}
(root/'network-probe-verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
if not complete:raise SystemExit('Inventory incomplete; entry removed and normal order restored')

"""Retrieve the physical update-policy report and clean up its one-time boot entry."""
import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
root = repo / 'artifacts/firmware'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
deployment = json.loads((root / 'policy-probe-deployment.json').read_text())
identity = repo / 'artifacts/ssh'
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'), '-o', 'BatchMode=yes',
       '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'), 'root@' + state['address']]
def run(command):
    result = subprocess.run(ssh+[command], capture_output=True, timeout=30)
    if result.returncode:
        raise SystemExit(result.stderr.decode(errors='replace'))
    return result.stdout
run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
    'test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"; '
    'rc-service sshd status; rc-service companion-watch status')
data = run('cat /boot/efi/EFI/companion/policy-probe-01.txt')
text = data.decode()
if not text.startswith('COMPANION_POLICY_PROBE_01\n') or not text.endswith('PROBE_COMPLETE\n'):
    raise SystemExit('Incomplete report')
lines = [line for line in text.splitlines() if line.startswith('UPDATE_POLICY ')]
if len(lines) != 5:
    raise SystemExit('Unexpected policy record count')
if 'signature_policy_byte=0x0000000000000001' not in lines[0]:
    raise SystemExit('Unexpected live policy; review before interpreting')
entry = deployment['entry']
if len(entry) != 4 or not all(c in '0123456789ABCDEFabcdef' for c in entry):
    raise SystemExit('Invalid entry')
boot = run('efibootmgr -v').decode()
matches = [line for line in boot.splitlines() if line.startswith('Boot'+entry)]
if len(matches) != 1 or 'Companion Policy Probe 01\t' not in matches[0] or '\\EFI\\companion\\policyprobex64.efi' not in matches[0]:
    raise SystemExit('Boot entry differs; no removal')
order = deployment['normal_boot_order'].removeprefix('BootOrder: ').strip()
if order != '0005,0000':
    raise SystemExit('Normal order differs; review')
clean = run('set -eu; efibootmgr --bootnum '+entry+' --delete-bootnum; efibootmgr --bootorder '+order+'; efibootmgr -v').decode()
if 'BootNext:' in clean or 'BootOrder: '+order not in clean:
    raise SystemExit('Boot cleanup failed')
(root / 'policy-probe-01.txt').write_bytes(data)
report = {'scope': __doc__, 'report_sha256': hashlib.sha256(data).hexdigest(),
          'efi_sha256': deployment['sha256'], 'boot_id': state['boot_id'],
          'root_reconnected_after_reboot': state['boot_id'] != deployment['previous_boot_id'],
          'healthy': True, 'policy_records': lines, 'normal_boot_order_restored': order,
          'temporary_entry_removed': entry, 'firmware_write_methods_called': False}
(root / 'policy-probe-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

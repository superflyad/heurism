"""Read selected Dell UEFI variables and management health; never write the target."""
import base64
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
global_guid = '8be4df61-93ca-11d2-aa0d-00e098032b8c'
owner = 'CompanionExtensionImage01-1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1'
command = r'''for f in /sys/firmware/efi/efivars/Boot*-8be4df61-93ca-11d2-aa0d-00e098032b8c /sys/firmware/efi/efivars/Driver*-8be4df61-93ca-11d2-aa0d-00e098032b8c /sys/firmware/efi/efivars/CompanionExtensionImage01-1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1; do
test -f "$f" || continue
printf '%s\t' "${f##*/}"
base64 "$f" | tr -d '\n'
printf '\n'
done'''
def read(command):
    result = subprocess.run([sys.executable, str(repo/'tools/dell.py'),
                             '--command', command, '--timeout', '30'],
                            capture_output=True, text=True, timeout=50)
    if result.returncode:
        raise RuntimeError(result.stderr + result.stdout)
    return result.stdout

variables = {}
for line in read(command).splitlines():
    name, encoded = line.split('\t', 1)
    raw = base64.b64decode(encoded, validate=True)
    if len(raw) < 4:
        raise ValueError('Truncated variable: '+name)
    variables[name] = {'base64': encoded, 'length': len(raw),
                       'sha256': hashlib.sha256(raw).hexdigest(),
                       'attributes': int.from_bytes(raw[:4], 'little')}
payload = base64.b64decode(variables[owner]['base64'])[4:]
assert hashlib.sha256(payload).hexdigest() == 'b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a'
def words(name):
    raw = base64.b64decode(variables[name+'-'+global_guid]['base64'])[4:]
    assert len(raw) % 2 == 0
    return [int.from_bytes(raw[i:i+2], 'little') for i in range(0, len(raw), 2)]
assert words('BootOrder') == [5, 0]
assert words('BootCurrent') in ([5], [0]), 'Not a verified management/fallback boot'
assert words('DriverOrder') == [0, 1]
assert 'BootNext-'+global_guid not in variables
health = read('cat /proc/sys/kernel/random/boot_id; cat /var/lib/companion/healthy-boot-id; rc-service sshd status; rc-service companion-watch status; sha256sum /boot/efi/EFI/alpine/grubx64.efi /boot/efi/EFI/boot/bootx64.efi /boot/efi/EFI/companion/recoveryx64.efi; cat /boot/efi/companion/recovery/grubenv')
lines = health.splitlines()
assert lines[0] == lines[1], 'Current boot has not been confirmed healthy'
assert health.count('5bc0e512af43def3ad39ce90b5084f1e56c73abc98d520e01466b7a7c7724efc') == 3
assert 'companion_pending=0' in health
output = repo/'artifacts/firmware/boot-variable-backups'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
output.mkdir(parents=True, exist_ok=False)
snapshot = {'captured_utc': datetime.now(timezone.utc).isoformat(),
            'scope': __doc__, 'variables': variables, 'health': health,
            'boot_order': words('BootOrder'), 'driver_order': words('DriverOrder'),
            'owner_payload_sha256': hashlib.sha256(payload).hexdigest()}
(output/'snapshot.json').write_text(json.dumps(snapshot, indent=2))
print(json.dumps({'backup': str(output/'snapshot.json'), 'variable_count': len(variables),
                  'boot_id': lines[0], 'owner_payload_verified': True,
                  'boot_and_recovery_files_verified': True}, indent=2))

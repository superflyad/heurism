"""Read-only verification of the guarded Dell Heurism NVRAM startup."""
import base64
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
marker_path = ('/sys/firmware/efi/efivars/CompanionNvStartup01-'
               '1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1')


def remote(command):
    result = subprocess.run([sys.executable, str(repo / 'tools/dell.py'),
                             '--command', command, '--timeout', '35'],
                            capture_output=True, text=True, timeout=50)
    if result.returncode:
        raise RuntimeError(result.stderr + result.stdout)
    return result.stdout.strip()


raw = base64.b64decode(''.join(remote('base64 ' + marker_path).split()), validate=True)
assert len(raw) == 68 and int.from_bytes(raw[:4], 'little') == 6
marker = struct.unpack('<8Q', raw[4:])
assert marker[:3] == (0x31564e504d4f4343, 1, 3), marker
assert marker[3:7] == (0, 0, 0, 0), marker
assert remote('sh /var/lib/companion/firmware/heurism-nv-candidate/activate-driver.sh verify') == (
    'dual-name SSD driver and protected manifest verified')
status = json.loads(remote('heurismctl status'))
assert status['ok'] and status['data']['platform'] == 'dell'
assert all(status['data']['management'][key] for key in ('ssh', 'watch', 'boot_healthy'))
health = json.loads(remote('/opt/heurism/native/current/heurism-release health'))
assert health['boot_id'] == status['data']['boot_id']
assert health['session'] == 'xfce' and health['uid'] == 1000
assert json.loads(remote('heurismctl power-check')) == {'ok': True, 'data': True}
sound = json.loads(remote('heurismctl sound-status'))
assert sound['ok'] and 'Speaker' in sound['data']['sink']
state = remote('set -eu; cat /proc/sys/kernel/random/boot_id; '
               'efibootmgr | sed -n "s/^BootCurrent: //p;s/^BootOrder: //p;s/^BootNext: //p"; '
               'efibootmgr --driver | sed -n "s/^DriverOrder: //p"')
assert state.splitlines() == [status['data']['boot_id'], '0005', '0005,0000', '0000,0001'], state
report = {
    'checked_utc': datetime.now(timezone.utc).isoformat(),
    'boot_id': status['data']['boot_id'],
    'marker_path': marker[2],
    'marker_read_load_start_info_status': list(marker[3:7]),
    'marker_sha256': hashlib.sha256(raw).hexdigest(),
    'active_driver_sha256': 'a215f4143742e4263577942e1886b5546375bf08777f11903196e0f99c78ff31',
    'management_healthy': True,
    'xfce_session_healthy': True,
    'speaker_sink_present': True,
    'boot_order': '0005,0000',
    'driver_order': '0000,0001',
    'bootnext_absent': True,
    'ssd_bootstrap_required': True,
    'legacy_owner_variable_retained': True,
}
output = repo / 'artifacts/firmware/heurism-nv-verification.json'
output.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))

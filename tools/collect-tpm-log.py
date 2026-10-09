"""Retrieve the current boot's TPM log over pinned root SSH; read only."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import struct

repo = Path(__file__).resolve().parents[1]
root = repo / 'artifacts/firmware'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
identity = repo / 'artifacts/ssh'
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'),
       '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5',
       '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'),
       'root@' + state['address']]

def read(command):
    result = subprocess.run(ssh + [command], capture_output=True, timeout=30)
    if result.returncode:
        raise SystemExit(result.stderr.decode(errors='replace'))
    return result.stdout

before = read('cat /proc/sys/kernel/random/boot_id').decode().strip()
data = read('cat /sys/kernel/security/tpm0/binary_bios_measurements')
after = read('cat /proc/sys/kernel/random/boot_id').decode().strip()
if before != after or not data or len(data) > 16 * 1024 * 1024:
    raise SystemExit('Boot changed or event log size invalid')
destination = root / ('tpm-log-' + before + '.bin')
if destination.exists() and destination.read_bytes() != data:
    raise SystemExit('Existing log for this boot differs; preserve it for investigation')
destination.write_bytes(data)
# TPM2_PCR_Read, SHA256 bank, PCR 0 only. This command does not alter a PCR.
response = read("python3 - <<'PY'\nimport os\nfd=os.open('/dev/tpmrm0',os.O_RDWR)\n"
                "try:\n os.write(fd,bytes.fromhex('8001000000140000017e00000001000b03010000'))\n"
                " print(os.read(fd,4096).hex())\nfinally:\n os.close(fd)\nPY")
response = bytes.fromhex(response.decode().strip())
if len(response) < 10:
    raise SystemExit('Truncated TPM response')
tag, length, code = struct.unpack('>HII', response[:10])
if tag != 0x8001 or length != len(response) or code != 0:
    raise SystemExit('PCR read failed: ' + hex(code))
final_boot = read('cat /proc/sys/kernel/random/boot_id').decode().strip()
if final_boot != before:
    raise SystemExit('Boot changed during PCR read')
destination.with_suffix('.pcr-read.bin').write_bytes(response)
metadata = {'boot_id': before, 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest(),
            'collected_utc': datetime.now(timezone.utc).isoformat(),
            'source': '/sys/kernel/security/tpm0/binary_bios_measurements',
            'scope': 'Reported boot measurements; not independent proof of enforcement',
            'pcr_read_response_sha256': hashlib.sha256(response).hexdigest()}
destination.with_suffix('.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
print(json.dumps(metadata, indent=2))

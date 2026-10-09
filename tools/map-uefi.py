"""Run a pinned, file-only UEFI parser on the verified BIOS backup."""
import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
root = repo / 'artifacts/firmware'
identity = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'), '-o', 'BatchMode=yes',
       '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes',
       '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'), 'root@' + state['address']]
remote = '/var/lib/companion/firmware/'
revision = 'dac91b26733ca21cb204e41614c1b8c81cc50860'

def run(command, timeout=30):
    result = subprocess.run(ssh + [command], capture_output=True, timeout=timeout)
    if result.returncode:
        raise SystemExit(result.stderr.decode(errors='replace') or 'Remote parser command failed')
    return result.stdout

head = run('git -C ' + remote + 'research/uefitool rev-parse HEAD').decode().strip()
if head != revision:
    raise SystemExit('Parser source revision mismatch')
image = (root / 'bios16-a.bin').read_bytes()
digest = hashlib.sha256(image).hexdigest()
metadata = json.loads((root / 'bios16-a.json').read_text())
if digest != metadata['sha256'] or len(image) != 0x1000000:
    raise SystemExit('Local BIOS backup size/hash mismatch')
if run('sha256sum ' + remote + 'bios16-a.bin').decode().split()[0] != digest:
    raise SystemExit('Remote BIOS backup differs')
parser_output = run(remote + 'research/uefitool-build/uefiextract ' + remote + 'bios16-a.bin report', 60)
report = run('cat ' + remote + 'bios16-a.bin.report.txt')
(root / 'uefi-parser.txt').write_bytes(parser_output)
(root / 'uefi-report.txt').write_bytes(report)
summary = {'parser_commit': revision, 'bios_sha256': digest,
           'source': 'https://github.com/LongSoft/UEFITool/tree/' + revision,
           'scope': 'Offline file parsing; no firmware writes or hardware acceptance test',
           'parser_output_sha256': hashlib.sha256(parser_output).hexdigest(),
           'report_sha256': hashlib.sha256(report).hexdigest(),
           'report_lines': len(report.splitlines())}
(root / 'uefi-map.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2))

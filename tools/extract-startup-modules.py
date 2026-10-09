"""Extract startup modules from the pinned backup; never execute vendor code."""
import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
root = repo / 'artifacts/firmware'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
ssh = ['ssh', '-i', str(repo / 'artifacts/ssh/companion_client_ed25519'),
       '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes',
       '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(repo / 'artifacts/ssh/known_hosts'), 'root@' + state['address']]
remote = '/var/lib/companion/firmware/'
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
revision = 'dac91b26733ca21cb204e41614c1b8c81cc50860'
if hashlib.sha256((root / 'bios16-a.bin').read_bytes()).hexdigest() != expected:
    raise SystemExit('Local ROM changed')

def read(command):
    result = subprocess.run(ssh + [command], capture_output=True, timeout=60)
    if result.returncode:
        raise SystemExit(result.stderr.decode(errors='replace') or result.stdout.decode(errors='replace')[-1500:] or 'Remote extraction failed')
    return result.stdout

if read('sha256sum ' + remote + 'bios16-a.bin').decode().split()[0] != expected:
    raise SystemExit('Remote ROM changed')
if read('git -C ' + remote + 'research/uefitool rev-parse HEAD').decode().strip() != revision:
    raise SystemExit('Parser changed')
modules = [('security-stub-dxe', 'F80697E9-7FD6-4665-8646-88E33EF71DFC'),
           ('dxe-ipl', '86D70125-BAA3-4296-A62F-602BEBBB9081'),
           ('dxe-core', 'D6A2CB7F-6A18-4E2F-B43B-9920A733700A')]
destination = root / 'update-modules'
destination.mkdir(exist_ok=True)
records = []
for name, guid in modules:
    folder = remote + 'research/startup-' + name + '-' + expected[:8]
    read('test -f ' + folder + '/body.bin || ' + remote + 'research/uefitool-build/uefiextract ' +
         remote + 'bios16-a.bin ' + guid + ' -o ' + folder + ' -m body -t 10')
    read('objdump -d ' + folder + '/body.bin >' + folder + '/disassembly.txt')
    for source, extension in [('body.bin', '.bin'), ('disassembly.txt', '.txt')]:
        content = read('cat ' + folder + '/' + source)
        target = destination / (name + extension)
        if target.exists() and target.read_bytes() != content:
            raise SystemExit('Existing artifact differs: ' + str(target))
        target.write_bytes(content)
        if extension == '.bin':
            records.append({'name': name, 'guid': guid, 'bytes': len(content),
                            'sha256': hashlib.sha256(content).hexdigest()})
report = {'scope': __doc__, 'bios_sha256': expected, 'parser_revision': revision, 'modules': records}
(root / 'startup-module-analysis.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

"""Extract update modules from the verified ROM as files; never execute them."""
import hashlib
import argparse
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
args_parser = argparse.ArgumentParser(description=__doc__)
args_parser.add_argument('--capsule', action='store_true', help='Extract the alternative DXE capsule/update route')
args = args_parser.parse_args()
root = repo / 'artifacts/firmware'
identity = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'),
       '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes',
       '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'), 'root@' + state['address']]
remote = '/var/lib/companion/firmware/'
revision = 'dac91b26733ca21cb204e41614c1b8c81cc50860'
expected = hashlib.sha256((root / 'bios16-a.bin').read_bytes()).hexdigest()

def read(command):
    process = subprocess.run(ssh + [command], capture_output=True, timeout=60)
    if process.returncode:
        raise SystemExit(process.stderr.decode(errors='replace') or
                         'Remote command failed (' + str(process.returncode) + '): ' + command)
    return process.stdout

if read('git -C ' + remote + 'research/uefitool rev-parse HEAD').decode().strip() != revision:
    raise SystemExit('Parser checkout changed')
if read('sha256sum ' + remote + 'bios16-a.bin').decode().split()[0] != expected:
    raise SystemExit('Remote BIOS differs')
modules = [('bios-guard-services', '6D4BAA0B-F431-4370-AF19-99D6209239F6'),
           ('dell-flash-recovery-pei', 'CF5014F8-2EBE-4D57-AA83-D7A3607371EA'),
           ('dell-flash-update-pei', '8DD46B11-0403-4B4C-B372-7041CB151834')]
if args.capsule:
    modules = [('capsule-runtime-dxe', '42857F0A-13F2-4B21-8A23-53D3F714B840'),
               ('dell-flash-update-dxe', 'F895B482-1970-49A7-84F5-723978086642'),
               ('recovery-image-rw-v2', '7305D9B2-95AE-4250-BD78-396C3B6AC2EE')]
records = []
destination = root / 'update-modules'
destination.mkdir(exist_ok=True)
for name, guid in modules:
    folder = remote + 'research/' + name + '-verified-' + expected[:8]
    log = read(remote + 'research/uefitool-build/uefiextract ' + remote +
               'bios16-a.bin ' + guid + ' -o ' + folder + ' -m body -t 10')
    read('objdump -d ' + folder + '/body.bin >' + folder + '/disassembly.txt')
    body = read('cat ' + folder + '/body.bin')
    disassembly = read('cat ' + folder + '/disassembly.txt')
    remote_hash = read('sha256sum ' + folder + '/body.bin').decode().split()[0]
    if hashlib.sha256(body).hexdigest() != remote_hash:
        raise SystemExit('Extracted module changed during download')
    (destination / (name + '.bin')).write_bytes(body)
    (destination / (name + '.txt')).write_bytes(disassembly)
    (destination / (name + '-parser.txt')).write_bytes(log)
    records.append({'name': name, 'guid': guid, 'bytes': len(body), 'sha256': remote_hash,
                    'disassembly_sha256': hashlib.sha256(disassembly).hexdigest()})
report = {'scope': 'Copied ROM modules/disassembly only; no hardware execution',
          'bios_sha256': expected, 'parser_revision': revision, 'modules': records}
(root / ('capsule-module-analysis.json' if args.capsule else 'update-module-analysis.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

"""Use a pinned upstream parser to inspect copied manifests, not live hardware."""
import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
identity = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'), '-o', 'BatchMode=yes',
    '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
    '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'), 'root@' + state['address']]
remote = '/var/lib/companion/firmware/'
research = remote + 'research/'
revision = '667ce65d75b23ce34df470bf56da8facacfbdb20'
head = subprocess.run(ssh + ['git -C ' + research + 'css rev-parse HEAD'], capture_output=True, text=True, timeout=15)
if head.returncode or head.stdout.strip() != revision:
    raise SystemExit('Upstream parser checkout does not match the reviewed revision')
root = repo / 'artifacts/firmware'
for name in ['key-manifest', 'boot-policy-manifest']:
    blob = (root / (name + '.bin')).read_bytes()
    uploaded = subprocess.run(ssh + ['umask 077; cat > ' + remote + name + '.bin'],
        input=blob, capture_output=True, timeout=15)
    digest = subprocess.run(ssh + ['sha256sum ' + remote + name + '.bin'],
        capture_output=True, text=True, timeout=15)
    if uploaded.returncode or digest.returncode or digest.stdout.split()[0] != hashlib.sha256(blob).hexdigest():
        raise SystemExit('Manifest transfer failed')
commands = {
    'km_verify': 'km-verify ' + remote + 'key-manifest.bin',
    'bpm_verify': 'bpm-verify ' + remote + 'boot-policy-manifest.bin',
    'km_show': 'km-show ' + remote + 'key-manifest.bin',
    'bpm_show': 'bpm-show ' + remote + 'boot-policy-manifest.bin',
    'read_config': 'read-config ' + remote + 'bootguard-config.json ' + remote + 'bios16-a.bin'}
report = {'parser_commit': revision,
    'source': 'https://github.com/9elements/converged-security-suite/tree/' + revision,
    'scope': 'Offline copied file analysis; signature verification against embedded public keys, '
             'not a comparison with hardware-fused key hash', 'commands': {}}
for name, command in commands.items():
    result = subprocess.run(ssh + [research + 'bg-prov ' + command], capture_output=True, text=True, timeout=60)
    report['commands'][name] = {'returncode': result.returncode,
                               'stdout': result.stdout, 'stderr': result.stderr}
if report['commands']['read_config']['returncode'] == 0:
    result = subprocess.run(ssh + ['cat ' + remote + 'bootguard-config.json'], capture_output=True, text=True, timeout=15)
    if result.returncode:
        raise SystemExit('Config download failed')
    config = json.loads(result.stdout)
    (root / 'bootguard-config.json').write_text(json.dumps(config, indent=2), encoding='utf-8')
    report['configuration_downloaded'] = True
(root / 'boot-policy-analysis.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'parser_commit': revision,
                  'results': {name: entry['returncode'] for name, entry in report['commands'].items()},
                  'configuration_downloaded': report.get('configuration_downloaded', False)}, indent=2))

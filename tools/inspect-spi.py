"""Run the read-only SPI inspection through pinned root SSH and save its report."""
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
identity = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
script = (repo / 'tools/target/companion-spi-inspect.py').read_text()
result = subprocess.run(['ssh', '-i', str(identity / 'companion_client_ed25519'),
    '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes',
    '-o', 'HostKeyAlias=companion-dell',
    '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'),
    'root@' + state['address'], 'python3 -'], input=script, capture_output=True, text=True, timeout=30)
if result.returncode:
    raise SystemExit(result.stderr)
report = json.loads(result.stdout)
stamp = report['utc'].split('.')[0].replace(':', '').replace('-', '').replace('+', '')
destination = repo / 'artifacts/firmware' / ('spi-inspection-' + stamp + '.json')
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(result.stdout)
print(destination)

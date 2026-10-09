"""Stage source over pinned SSH; installation/start remain explicit guarded commands."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tarfile

repo = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(repo/'tools/dell.py'), '--command',
                'mkdir -p /var/lib/companion/desktop-stage'], check=True)
out = repo/'build/desktop'
out.mkdir(parents=True, exist_ok=True)
archive = out/'desktop.tar.gz'
files = list((repo/'userspace').glob('*.py')) + list((repo/'userspace').glob('*.sh'))
files += list((repo/'userspace').glob('*.html')) + list((repo/'userspace').glob('*.xml'))
files += list((repo/'userspace').glob('*.initd')) + list((repo/'tests').glob('desktop*.py'))
with tarfile.open(archive, 'w:gz') as tar:
    for path in files:
        tar.add(path, arcname=path.name)
manifest = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
(out/'source-hashes.json').write_text(json.dumps(manifest, indent=2))
state = json.loads((repo/'artifacts/targets/dell.json').read_text())
identity = repo/'artifacts/ssh'
options = ['-i', str(identity/'companion_client_ed25519'), '-o', 'BatchMode=yes',
           '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
           '-o', 'UserKnownHostsFile='+str(identity/'known_hosts')]
subprocess.run(['scp', *options, str(archive), 'root@'+state['address']+
                ':/var/lib/companion/desktop-stage/desktop.tar.gz'], check=True, timeout=30)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
subprocess.run([sys.executable, str(repo/'tools/dell.py'), '--command',
               f"printf '%s\\n' '{digest}  /var/lib/companion/desktop-stage/desktop.tar.gz' | sha256sum -c -"], check=True)
print('Desktop staged; not started.')

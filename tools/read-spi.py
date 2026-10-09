"""Compile and execute the restricted SPI flash reader over pinned root SSH."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('offset', type=lambda value: int(value, 0))
parser.add_argument('length', type=lambda value: int(value, 0))
parser.add_argument('name', help='Unique output name, without an extension')
args = parser.parse_args()
if not re.fullmatch(r'[a-z0-9-]{1,64}', args.name):
    raise SystemExit('Invalid output name')
if args.offset < 0 or args.length <= 0 or args.offset + args.length > 24*1024*1024 or args.offset % 64 or args.length % 64:
    raise SystemExit('Invalid bounded/aligned range')
repo = Path(__file__).resolve().parents[1]
identity = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'), '-o', 'BatchMode=yes',
    '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
    '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'),
    '-o', 'ServerAliveInterval=10', '-o', 'ServerAliveCountMax=3', 'root@' + state['address']]
source = (repo / 'tools/target/companion-spi-read.c').read_bytes()
remote = '/var/lib/companion/firmware/' + args.name + '.bin'
command = 'umask 077; mkdir -p /etc/companion/tools /var/lib/companion/firmware && cat > /etc/companion/tools/companion-spi-read.c && gcc -std=c11 -O2 -Wall -Wextra -Werror /etc/companion/tools/companion-spi-read.c -o /etc/companion/tools/companion-spi-read && /etc/companion/tools/companion-spi-read ' + f'{args.offset} {args.length} {remote}'
result = subprocess.run(ssh + [command], input=source, capture_output=True, timeout=300)
if result.returncode:
    raise SystemExit(result.stderr.decode(errors='replace') + '\nNo complete backup created; any target file is partial.')
download = subprocess.run(ssh + ['cat ' + remote], capture_output=True, timeout=90)
if download.returncode or len(download.stdout) != args.length:
    raise SystemExit('Backup download failed or size mismatch')
digest = hashlib.sha256(download.stdout).hexdigest()
verification = subprocess.run(ssh + ['sha256sum ' + remote], capture_output=True, text=True, timeout=15)
if verification.returncode or verification.stdout.split()[0] != digest:
    raise SystemExit('Backup SHA-256 mismatch')
destination = repo / 'artifacts/firmware' / (args.name + '.bin')
if destination.exists():
    raise SystemExit('Local destination exists; will not overwrite')
destination.write_bytes(download.stdout)
metadata = {'address': state['address'], 'offset': args.offset, 'bytes': args.length,
    'sha256': digest, 'reader_source_sha256': hashlib.sha256(source).hexdigest(),
    'remote_path': remote, 'stderr': result.stderr.decode(errors='replace'),
    'scope': 'Hardware READ cycles only; controller address/control changed to request reads; no flash contents written'}
destination.with_suffix('.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
print(json.dumps(metadata, indent=2))
print(destination)

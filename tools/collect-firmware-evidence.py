"""Collect bounded, read-only board evidence through the pinned root SSH connection.

No flash chip probing, MMIO access, register writes or firmware updates.
ACPI licensing tables and SSH/private credentials are not collected.
"""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
identity = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'),
       '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5',
       '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'),
       '-o', 'ServerAliveInterval=10', '-o', 'ServerAliveCountMax=3',
       'root@' + state['address'], 'python3 -']
remote = r'''
import base64, hashlib, json, os, subprocess
from pathlib import Path
assert os.getuid() == 0
assert Path('/etc/hostname').read_text().strip() == 'companion-dell'
files = {}
errors = {}
def collect(path, maximum=2*1024*1024):
    try:
        with Path(path).open('rb') as stream:
            data = stream.read(maximum + 1)
        if len(data) > maximum:
            raise ValueError('collection limit exceeded')
        files[path] = {'sha256': hashlib.sha256(data).hexdigest(),
                       'bytes': len(data), 'base64': base64.b64encode(data).decode()}
    except (OSError, ValueError) as error:
        errors[path] = str(error)
for path in ['/proc/cpuinfo', '/proc/iomem', '/proc/cmdline',
             '/proc/sys/kernel/random/boot_id', '/proc/version',
             '/sys/class/mei/mei0/fw_ver', '/sys/class/watchdog/watchdog0/state']:
    collect(path)
for directory in [Path('/sys/firmware/acpi/tables'), Path('/sys/firmware/acpi/tables/dynamic')]:
    for path in sorted(directory.iterdir()):
        if path.is_file() and path.name not in ('MSDM', 'SLIC'):
            collect(str(path))
for address in ['0000:00:1f.0', '0000:00:1f.4', '0000:00:1f.5', '0000:00:16.0']:
    base = Path('/sys/bus/pci/devices') / address
    for field in ['vendor', 'device', 'subsystem_vendor', 'subsystem_device', 'resource']:
        collect(str(base / field))
    try:
        with (base / 'config').open('rb') as stream:
            data = stream.read(256)
        files[str(base / 'config')] = {'sha256': hashlib.sha256(data).hexdigest(),
            'bytes': len(data), 'base64': base64.b64encode(data).decode()}
    except OSError as error:
        errors[str(base / 'config')] = str(error)
commands = {}
for argv in [['lspci', '-nn'], ['dmesg'], ['efibootmgr', '-v'],
             ['companion-firmware-audit']]:
    result = subprocess.run(argv, capture_output=True, timeout=30)
    commands[' '.join(argv)] = {'returncode': result.returncode,
        'stdout': result.stdout.decode(errors='replace')[:2*1024*1024],
        'stderr': result.stderr.decode(errors='replace')[:16384]}
print(json.dumps({'files': files, 'commands': commands, 'errors': errors}))
'''
result = subprocess.run(ssh, input=remote, text=True, capture_output=True, timeout=150)
if result.returncode:
    raise SystemExit(result.stderr or 'Remote evidence collection failed')
report = json.loads(result.stdout)
if report['commands']['companion-firmware-audit']['returncode']:
    raise SystemExit('Firmware audit did not complete')
report['collected_utc'] = datetime.now(timezone.utc).isoformat()
report['address'] = state['address']
report['scope'] = 'OS-exported board evidence only; not a flash backup or complete fuse audit'
destination = repo / 'artifacts/firmware' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
destination.mkdir(parents=True, exist_ok=False)
import base64
for name, entry in report['files'].items():
    data = base64.b64decode(entry.pop('base64'), validate=True)
    if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise SystemExit('Evidence integrity mismatch: ' + name)
    relative = name.lstrip('/').replace(':', '_')
    entry['local_path'] = relative
    local = destination / relative
    local.parent.mkdir(parents=True, exist_ok=True)
    local.write_bytes(data)
(destination / 'evidence.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(destination)
print(f"Collected {len(report['files'])} files; {len(report['errors'])} unavailable")

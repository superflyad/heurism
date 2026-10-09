"""Copy a tested provisioning payload onto an already-validated FAT32 USB."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
from datetime import datetime, timezone

repo = Path(__file__).resolve().parents[1]
payload = repo / 'build' / 'provisioning' / 'payload'
root = Path(sys.argv[1]).resolve()
assert root.is_dir() and root == Path(root.anchor), 'Expected a volume root'
assert root.drive.upper() not in ('C:', 'D:', 'E:'), 'Refusing known internal development volumes'
manifest = json.loads((payload / 'companion-manifest.json').read_text())
files = sorted(p for p in payload.rglob('*') if p.is_file())
needed = sum(p.stat().st_size for p in files)
assert shutil.disk_usage(root).free > needed * 1.1, 'Insufficient USB space for payload and backups'
stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
local_backup = repo / 'artifacts' / 'usb-backup' / ('provisioning-' + stamp)
usb_backup = root / 'Companion-backup' / ('provisioning-' + stamp)
local_backup.mkdir(parents=True); usb_backup.mkdir(parents=True)

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as handle:
        while data := handle.read(4 * 1024**2): h.update(data)
    return h.hexdigest()

backups = []
for number, source in enumerate(files, 1):
    relative = source.relative_to(payload)
    target = root / relative
    assert target.resolve().is_relative_to(root), 'Target escaped USB root'
    expected = manifest['files'].get(relative.as_posix(), sha(source))
    assert sha(source) == expected, f'Source manifest mismatch: {relative}'
    old_hash = sha(target) if target.exists() else None
    if old_hash is not None and old_hash != expected:
        for backup_root in (local_backup, usb_backup):
            destination = backup_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(target, destination)
            assert sha(destination) == old_hash, f'Backup failed: {relative}'
        backups.append({'path': relative.as_posix(), 'sha256': old_hash})
    if old_hash != expected:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        assert sha(target) == expected, f'USB verification failed: {relative}'
    if number % 100 == 0:
        print(f'Verified {number}/{len(files)} files', flush=True)
record = {'prepared_utc': datetime.now(timezone.utc).isoformat(), 'usb_root': str(root),
          'files_verified': len(files), 'payload_bytes': needed, 'previous_files': backups,
          'local_backup': str(local_backup), 'usb_backup': str(usb_backup),
          'firmware_flashed': False, 'internal_disks_modified': False}
for backup_root in (local_backup, usb_backup):
    (backup_root / 'preparation.json').write_text(json.dumps(record, indent=2))
(root / 'COMPANION-BOOT.txt').write_text('''COMPANION PROVISIONING 0.1
Boot the first entry: Companion provisioning - remote access and installer.
Connect Ethernet to the same router for automatic DHCP, or log in locally as
root (no local password) and run companion-wifi. SSH uses authorized keys only.
Run companion-status to see the address; send that address to this workspace.

USB boot inventories hardware and starts remote root access. It does not erase
the internal drive. companion-install previews available targets. Its explicit
--erase command installs a persistent Alpine-based management system, replacing
all existing partitions on the chosen disk. Native Companion kernel pending.

Motherboard firmware is not flashed: support and recovery remain unestablished.
The second boot-menu entry retains the original Companion UEFI experiment.
Secure Boot must remain disabled for these development images.
''')
print(json.dumps(record, indent=2))

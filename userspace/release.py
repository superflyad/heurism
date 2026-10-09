"""Local release integrity, initial UI health and reversible application rollback."""
import hashlib
import json
import os
from pathlib import Path
import pwd
import sys

ROOT = Path('/opt/companion/releases')
LINK = Path('/opt/companion/desktop')
PREVIOUS = Path('/var/lib/companion/desktop-previous-release')
REQUIRED = {'control.py', 'shell.py', 'applications.py', 'system_actions.py', 'home.py',
            'input_settings.py', 'session-config.py', 'session.sh', 'client.sh',
            'user-session.sh', 'release.py', 'platform_config.py', 'openbox.xml', 'start.html', 'control.initd', 'desktop.initd'}


def checked(path):
    path = Path(path).resolve(strict=True)
    if path.parent != ROOT or not path.is_dir() or path.stat().st_uid != 0:
        raise ValueError('Invalid release path')
    return path


def seal(path):
    path = checked(path)
    values = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in path.iterdir()
              if p.is_file() and p.name != 'hashes.json'}
    (path/'hashes.json').write_text(json.dumps(values, indent=2))


def verify(path):
    path = checked(path)
    values = json.loads((path/'hashes.json').read_text())
    if not isinstance(values, dict) or not REQUIRED.issubset(values):
        raise ValueError('Release manifest is incomplete')
    for name, digest in values.items():
        item = path/name
        if (Path(name).name != name or item.is_symlink() or item.stat().st_uid != 0
            or item.stat().st_mode & 0o022 or hashlib.sha256(item.read_bytes()).hexdigest() != digest):
            raise ValueError('Release integrity failed')
    return path


def rollback():
    previous = checked(PREVIOUS.read_text().strip())
    # Legacy 0.2 predates the sealed release layout; preserve that known fallback.
    if not previous.name.startswith('legacy-'):
        verify(previous)
    current = checked(LINK)
    temporary = LINK.with_name('desktop.rollback')
    temporary.unlink(missing_ok=True)
    temporary.symlink_to(previous)
    temporary.replace(LINK)
    PREVIOUS.write_text(str(current)+'\n')
    print(json.dumps({'previous': str(current), 'active': str(previous)}))


def health():
    path = checked(LINK)
    info = json.loads(Path('/var/lib/companion/desktop-user/.local/state/companion/session-health.json').read_text())
    pid = int(info['pid'])
    process = Path('/proc')/str(pid)
    if (info['release'] != str(path) or process.stat().st_uid != pwd.getpwnam('companion-ui').pw_uid
        or b'/opt/companion/desktop/shell.py' not in (process/'cmdline').read_bytes().split(b'\0')
        or info['boot_id'] != Path('/proc/sys/kernel/random/boot_id').read_text().strip()):
        raise ValueError('UI is not healthy for the active release')
    print(json.dumps(info))


if __name__ == '__main__':
    try:
        action = sys.argv[1]
        if action == 'seal':
            seal(sys.argv[2])
        elif action == 'verify':
            print(verify(sys.argv[2] if len(sys.argv)>2 else LINK))
        elif action == 'rollback':
            rollback()
        elif action == 'health':
            health()
        else:
            raise ValueError('Unknown release operation')
    except (OSError, ValueError, KeyError, IndexError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)

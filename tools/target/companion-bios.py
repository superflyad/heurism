#!/usr/bin/python3
"""Read supported Dell BIOS settings; change only an exposed enum value (Python 3)."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re

base = Path('/sys/class/firmware-attributes/dell-wmi-sysman/attributes')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('action', choices=['list', 'get', 'set'])
parser.add_argument('name', nargs='?')
parser.add_argument('value', nargs='?')
args = parser.parse_args()
def read(path):
    try:
        return path.read_text().strip()
    except OSError:
        return None
def setting(path):
    return {key: read(path / key) for key in
            ['display_name', 'type', 'current_value', 'default_value', 'possible_values']}
if args.action == 'list':
    if args.name or args.value:
        parser.error('list takes no name or value')
    result = {path.name: setting(path) for path in sorted(base.iterdir())
              if path.is_dir() and (path / 'current_value').exists()}
else:
    if not args.name or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', args.name):
        parser.error('provide an exact supported setting name')
    path = base / args.name
    if not (path / 'current_value').is_file():
        parser.error('setting is unavailable')
    result = setting(path)
    if args.action == 'set':
        allowed = [v for v in (result['possible_values'] or '').split(';') if v]
        if not args.value or args.value not in allowed:
            parser.error('value must exactly match an exposed enum: ' + ', '.join(allowed))
        previous = result['current_value']
        if previous != args.value:
            # Save intent before changing the firmware setting, including failures.
            log = Path('/var/lib/companion/bios-changes.jsonl')
            log.parent.mkdir(parents=True, exist_ok=True)
            with log.open('a') as record:
                os.chmod(log, 0o600)
                record.write(json.dumps({'utc': datetime.now(timezone.utc).isoformat(),
                    'name': args.name, 'previous': previous, 'requested': args.value}) + '\n')
            (path / 'current_value').write_text(args.value)
        result = setting(path) | {'previous': previous,
            'pending_reboot': read(base / 'pending_reboot')}
    elif args.value:
        parser.error('get takes no value')
print(json.dumps(result, indent=2))

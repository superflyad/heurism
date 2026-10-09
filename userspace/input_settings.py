"""Validated fixed-device X input settings, shared by control and session startup."""
import json
import math
import os
from pathlib import Path
import subprocess
import platform_config

DEFAULTS = {'tap': True, 'natural_scroll': True, 'speed': 0.0}
ENV = dict(os.environ, DISPLAY=':0', XAUTHORITY='/run/companion-desktop/Xauthority')


def validate(values):
    if not isinstance(values, dict) or not values or set(values) - set(DEFAULTS):
        raise ValueError('Unknown input setting')
    for name, value in values.items():
        if name == 'speed':
            if type(value) not in (int, float) or not math.isfinite(value) or not -1 <= value <= 1:
                raise ValueError('Pointer speed must be between -1 and 1')
        elif type(value) is not bool:
            raise ValueError('Input switches require true or false')
    return values


def effective():
    result = subprocess.run(['xinput', 'list-props', 'touchpad'], env=ENV,
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=2)
    if result.returncode:
        return None
    found = {}
    mapping = {'libinput Tapping Enabled (': 'tap', 'libinput Natural Scrolling Enabled (': 'natural_scroll',
               'libinput Accel Speed (': 'speed', 'libinput Disable While Typing Enabled (': 'typing_rejection',
               'libinput Scroll Method Enabled (': 'scroll_method'}
    for line in result.stdout.splitlines():
        for property_, name in mapping.items():
            if line.strip().startswith(property_):
                value = line.split(':', 1)[1].strip()
                found[name] = (float(value) if name == 'speed' else [int(v) for v in value.split(',')]
                               if name == 'scroll_method' else bool(int(value)))
    return found or None


def apply(values):
    validate(values)
    properties = {'tap': 'libinput Tapping Enabled', 'natural_scroll': 'libinput Natural Scrolling Enabled',
                  'speed': 'libinput Accel Speed'}
    previous = effective()
    if previous is None or not all(name in previous for name in values):
        raise ValueError('The libinput touchpad is not ready')
    changed = []
    try:
        for name, value in values.items():
            subprocess.run(['xinput', 'set-prop', 'touchpad', properties[name],
                            str(int(value)) if type(value) is bool else str(value)],
                           env=ENV, check=True, timeout=2, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            changed.append(name)
        current = effective()
        if current is None or any(abs(float(current[name])-float(value)) > 0.001 for name, value in values.items()):
            raise ValueError('Input setting verification failed')
        return current
    except (OSError, subprocess.SubprocessError, ValueError):
        for name in changed:
            try:
                subprocess.run(['xinput', 'set-prop', 'touchpad', properties[name], str(float(previous[name]))],
                               env=ENV, timeout=2, check=True, stderr=subprocess.DEVNULL)
            except (OSError, subprocess.SubprocessError):
                pass
        raise ValueError('Input settings could not be applied')


if __name__ == '__main__':
    preferences = Path('/var/lib/companion/desktop/preferences.json')
    values = dict(DEFAULTS)
    try:
        stored = json.loads(preferences.read_text()).get('input', {})
        if stored:
            values.update(validate(stored))
    except (OSError, ValueError, AttributeError):
        pass
    if platform_config.profile() == 'hyperv-dev' and effective() is None:
        print(json.dumps({'touchpad': 'unavailable on this VM; virtual keyboard and pointer active'}), flush=True)
    else:
        print(json.dumps(apply(values)), flush=True)

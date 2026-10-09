"""Bounded Dell UI/control checks; restore appearance and brightness, never reboot."""
import json
import os
from pathlib import Path
import pwd
import signal
import subprocess
import sys
import time

sys.path.insert(0, '/opt/companion/desktop')
from shell import request

env = dict(os.environ, DISPLAY=':0', XAUTHORITY='/run/companion-desktop/Xauthority')
uid = pwd.getpwnam('companion-ui').pw_uid

def ui_pids():
    found = []
    for path in Path('/proc').glob('[0-9]*'):
        try:
            args = (path/'cmdline').read_bytes().split(b'\0')
            if path.stat().st_uid == uid and b'/opt/companion/desktop/shell.py' in args:
                found.append(int(path.name))
        except OSError:
            pass
    return found

before = request()
old = ui_pids()
assert len(old) == 1
original_theme = before['preferences']['theme']
original_light = before['brightness']
geometry = subprocess.check_output(['xdotool', 'getdisplaygeometry'], env=env).decode().split()
width, height = map(int, geometry)
scale = min(width/1280, height/800)
offset = (width-1280*scale)/2

try:
    target = 90 if original_light >= 95 else min(100, original_light+5)
    reply = request('brightness', target)
    assert abs(reply['brightness']-target) <= 1
    actual = Path('/sys/class/backlight/intel_backlight')
    level = round(100*int((actual/'brightness').read_text())/int((actual/'max_brightness').read_text()))
    assert abs(level-target) <= 1
    request('brightness', original_light)
    windows = subprocess.check_output(['xdotool', 'search', '--name', '^Companion$'], env=env).decode().split()
    subprocess.run(['xdotool', 'windowfocus', windows[0], 'key', 'F1'], env=env, check=True)
    time.sleep(1)
    subprocess.run(['xdotool', 'mousemove', str(round(offset+785*scale)), str(round(572*scale)),
                    'click', '1'], env=env, check=True)
    for _ in range(12):
        time.sleep(0.5)
        if request()['preferences']['theme'] != original_theme:
            break
    assert request()['preferences']['theme'] != original_theme
    request('theme', original_theme)
    # Simulate a crashed UI. Only the exact unprivileged shell PID is killed.
    os.kill(old[0], signal.SIGTERM)
    fresh = []
    for _ in range(25):
        time.sleep(1)
        fresh = ui_pids()
        if fresh and fresh != old:
            time.sleep(2)
            break
    assert len(fresh) == 1 and fresh != old, 'Shell did not recover'
    windows = subprocess.check_output(['xdotool', 'search', '--name', '^Companion$'], env=env).decode().split()
    assert windows
    after = request()
    assert after['management']['ssh'] and after['management']['watch']
    assert after['boot_id'] == before['boot_id']
    result = {'ok': True, 'boot_id': after['boot_id'], 'resolution': [width, height],
              'ui_uid': uid, 'old_ui_pids': old, 'new_ui_pids': fresh,
              'real_backlight_write_read_restore': True, 'physical_x_pointer_action': True,
              'management_after_ui_crash': after['management'], 'state': after}
    Path('/var/lib/companion/desktop-stage/evidence/physical-check.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
finally:
    request('brightness', original_light)
    request('theme', original_theme)

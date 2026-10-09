"""Installed Linux API and real Tk/Xvfb integration, no reboot/network changes."""
import json
import os
from pathlib import Path
import pwd
import secrets
import socket
import subprocess
import sys
import time

sys.path.insert(0, '/opt/companion/desktop')
from shell import request

out = Path('/var/lib/companion/desktop-stage/evidence')
out.mkdir(exist_ok=True)
account = pwd.getpwnam('companion-ui')
auth = Path('/run/companion-desktop/test-Xauthority')
auth.touch(mode=0o640)
os.chown(auth, 0, account.pw_gid)
subprocess.run(['xauth', '-f', str(auth), 'add', ':99', '.', secrets.token_hex(16)], check=True)
os.chown(auth, 0, account.pw_gid)
os.chmod(auth, 0o640)
env = dict(os.environ, DISPLAY=':99', XAUTHORITY=str(auth))
server = subprocess.Popen(['Xvfb', ':99', '-screen', '0', '1280x800x24', '-nolisten', 'tcp', '-auth', str(auth)],
                          stdout=(out/'xvfb.log').open('w'), stderr=subprocess.STDOUT)
ui = None
original = request()['preferences']['theme']
try:
    for _ in range(30):
        if Path('/tmp/.X11-unix/X99').exists():
            break
        time.sleep(0.1)
    ui = subprocess.Popen(['su', '-s', '/bin/sh', '-c',
                           f'DISPLAY=:99 XAUTHORITY={auth} exec python3 /opt/companion/desktop/shell.py',
                           'companion-ui'], stdout=(out/'ui-test.log').open('w'), stderr=subprocess.STDOUT)
    time.sleep(3)
    assert ui.poll() is None, 'UI exited'
    windows = subprocess.check_output(['xdotool', 'search', '--name', '^Companion$'], env=env).decode().split()
    assert windows, 'No Companion window'
    before = request()
    assert before['management']['ssh'] and before['management']['watch'] and before['network']
    subprocess.run(['xdotool', 'windowfocus', windows[0], 'key', 'F1'], env=env, check=True)
    time.sleep(1)
    subprocess.run(['xdotool', 'mousemove', '785', '572', 'click', '1'], env=env, check=True)
    time.sleep(3)
    changed = request()['preferences']['theme']
    assert changed != original, 'Pointer action did not save theme'
    subprocess.run(['xwd', '-root', '-silent', '-out', str(out/'preview.xwd')], env=env, check=True)
    # Exercise a keyboard event in the actual Tk process.
    subprocess.run(['xdotool', 'windowfocus', windows[0], 'key', 'F5'], env=env, check=True)
    for req in ({'version': 1, 'action': 'brightness', 'value': 0},
                {'version': 1, 'action': 'command', 'value': 'reboot'}, None):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(3)
            client.connect('/run/companion-desktop/control.sock')
            client.sendall(json.dumps(req).encode()+b'\n')
            assert not json.loads(client.makefile('rb').readline())['ok']
    subprocess.run(['xdotool', 'key', 'ctrl+q'], env=env, check=True)
    assert ui.wait(timeout=5) == 0
    after = request()
    assert after['management']['ssh'] and after['management']['watch']
    assert after['boot_id'] == before['boot_id']
    (out/'integration.json').write_text(json.dumps({'ok': True, 'boot_id': after['boot_id'],
        'window': windows[0], 'theme_pointer_action': True, 'keyboard_exit': True,
        'invalid_operations_rejected': True, 'management_after_ui_exit': after['management']}, indent=2))
    print((out/'integration.json').read_text())
finally:
    request('theme', original)
    if ui and ui.poll() is None:
        ui.terminate()
    server.terminate()
    server.wait(timeout=5)
    auth.unlink(missing_ok=True)

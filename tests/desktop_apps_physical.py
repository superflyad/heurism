"""Physical X session app launches, actual Onboard text events and root console proof."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, '/opt/companion/desktop')
from shell import request

STATE = Path('/var/lib/companion/desktop-user/.local/state/companion')
OUT = Path('/var/lib/companion/desktop-stage/evidence')
ENV = dict(os.environ, DISPLAY=':0', XAUTHORITY='/run/companion-desktop/Xauthority')


def x(*args):
    return subprocess.check_output(['xdotool', *map(str, args)], env=ENV, text=True).strip()


def window(name):
    for _ in range(40):
        found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', name],
                               env=ENV, capture_output=True, text=True)
        if found.stdout.split():
            return found.stdout.split()[-1]
        time.sleep(0.25)
    raise AssertionError('Physical window not found: '+name)


def typing():
    import tkinter as tk
    root = tk.Tk()
    root.title('Companion typing verification')
    root.geometry('600x140+620+280')
    entry = tk.Entry(root, font=('DejaVu Sans', 26))
    entry.pack(fill='x', padx=20, pady=25)
    entry.focus_force()
    def finish():
        (STATE/'typing-proof.json').write_text(json.dumps({'text': entry.get(), 'uid': os.getuid()}))
        root.destroy()
    root.after(7000, finish)
    root.mainloop()


def main():
    if '--typing' in sys.argv:
        typing()
        return
    for _ in range(30):
        ready = subprocess.run(['python3', '/usr/local/sbin/companion-release', 'health'],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if ready.returncode == 0:
            break
        time.sleep(0.5)
    assert ready.returncode == 0, 'Physical shell is not ready'
    shell = window('^Companion$')
    width, height = map(int, x('getdisplaygeometry').split())
    scale = min(width/1280, height/800)
    ox, oy = (width-1280*scale)/2, (height-800*scale)/2
    def home():
        showing = subprocess.check_output(['xprop', '-root', '_NET_SHOWING_DESKTOP'], env=ENV, text=True)
        if not showing.strip().endswith('= 1'):
            x('key', 'super+d')
        x('windowfocus', '--sync', shell, 'key', 'F4')
        time.sleep(0.4)
    def launch(cx, cy, title):
        home()
        x('mousemove', round(ox+cx*scale), round(oy+cy*scale), 'click', '1')
        return window(title)
    before = request()
    def close(target):
        x('windowactivate', '--sync', target, 'key', 'alt+F4')
        time.sleep(0.3)
    files = launch(120, 315, 'Files$')
    close(files)
    editor = launch(270, 315, 'Editor$')
    close(editor)
    terminal = launch(270, 440, '^Companion Terminal$')
    report = STATE/'terminal-physical-uid.txt'
    report.unlink(missing_ok=True)
    x('windowactivate', '--sync', terminal)
    time.sleep(0.6)  # A mapped xterm can precede its PTY/shell accepting input.
    x('type', '--clearmodifiers', f'id -u > {report}')
    x('key', 'Return')
    for _ in range(20):
        if report.exists():
            break
        time.sleep(0.1)
    assert report.read_text().strip() == '1000'
    close(terminal)
    browser = launch(120, 440, 'Companion.*Mozilla Firefox')
    close(browser)
    keyboard = launch(120, 565, '^Onboard$')
    proof = STATE/'typing-proof.json'
    proof.unlink(missing_ok=True)
    child = subprocess.Popen(['su', '-s', '/bin/sh', '-c',
        f'DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority '
        f'exec python3 {Path(__file__).resolve()} --typing', 'companion-ui'])
    entry = window('^Companion typing verification$')
    x('windowactivate', '--sync', entry)
    # The actual Compact keyboard's Q key: click its visible widget instead of
    # sending a synthetic keyboard key directly to the verification entry.
    geometry = x('getwindowgeometry', '--shell', keyboard)
    values = dict(line.split('=', 1) for line in geometry.splitlines() if '=' in line)
    x('mousemove', int(values['X'])+158, int(values['Y'])+97, 'click', '1')
    assert child.wait(timeout=12) == 0
    assert json.loads(proof.read_text()) == {'text': 'q', 'uid': 1000}, proof.read_text()
    # Close the actual keyboard via its own UI Close control.
    x('mousemove', int(values['X'])+1145, int(values['Y'])+35, 'click', '1')
    admin = request('admin-console', 'admin')
    admin_window = window('^Companion Administrator$')
    assert Path('/proc') .joinpath(str(admin['pid'])).stat().st_uid == 0
    admin_report = OUT/'admin-console-uid.txt'
    admin_report.unlink(missing_ok=True)
    x('windowactivate', '--sync', admin_window)
    time.sleep(0.6)
    x('type', '--clearmodifiers', f'id -u > {admin_report}')
    x('key', 'Return')
    for _ in range(20):
        if admin_report.exists():
            break
        time.sleep(0.1)
    assert admin_report.read_text().strip() == '0'
    close(admin_window)
    home()
    after = request()
    assert after['boot_id'] == before['boot_id']
    assert after['management'] == {'ssh': True, 'watch': True, 'boot_healthy': True}
    virtual = before.get('platform') == 'hyperv-dev'
    result = {'ok': True, 'boot_id': after['boot_id'], 'virtual_hardware': virtual, 'x_session_launches':
              ['files', 'editor', 'terminal', 'browser', 'keyboard'],
              'terminal_uid': 1000, 'administrator_uid': 0,
              'onboard_pointer_click_generated_text': 'q', 'management_healthy': True,
              'physical_finger_or_touchpad_motion_observed': False}
    (OUT/('vm-apps.json' if virtual else 'physical-apps.json')).write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

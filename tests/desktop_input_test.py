"""Actual Tk gesture/navigation tests on Xvfb, with live libinput property checks."""
import json
import os
from pathlib import Path
import pwd
import secrets
import subprocess
import sys
import time

sys.path.insert(0, '/opt/companion/desktop')
from shell import Shell, request


def client():
    original = request()['preferences']['input']
    shell = Shell()
    def pump(seconds=0.2):
        end = time.monotonic()+seconds
        while time.monotonic() < end:
            shell.root.update()
            time.sleep(0.02)
    def event(name, x=0, y=0):
        shell.canvas.event_generate(name, x=x, y=y)
        pump()
    try:
        for _ in range(30):
            pump()
            if shell.data and shell.input_state:
                break
        assert shell.data and shell.input_state
        shell.root.event_generate('<Tab>')
        pump()
        shell.root.event_generate('<Tab>')
        pump()
        shell.root.event_generate('<Tab>')
        pump()
        shell.root.event_generate('<Return>')
        pump()
        assert shell.page == 'input', 'Keyboard page navigation failed'
        event('<ButtonPress-1>', 150, 320)
        pump(1.5)  # Periodic redraw while pressed must not lose the action.
        event('<ButtonRelease-1>', 150, 320)
        pump(3)
        assert request('input-status')['tap'] != original['tap']
        assert request()['preferences']['input']['tap'] != original['tap']
        current = request('input-status')['tap']
        event('<ButtonPress-1>', 150, 320)
        event('<B1-Motion>', 450, 440)
        event('<ButtonRelease-1>', 450, 440)
        pump(2)
        assert request('input-status')['tap'] == current, 'Drag activated a button'
        event('<ButtonPress-1>', 700, 350)
        event('<B1-Motion>', 800, 360)
        event('<B1-Motion>', 950, 410)
        event('<ButtonRelease-1>', 950, 410)
        points = len(shell.ink)
        assert points >= 4
        pump(2)
        assert len(shell.ink) == points, 'Polling erased touch drawing'
        event('<Button-5>', 850, 550)
        assert shell.scroll_position == 1, 'Scrolling failed'
        event('<Button-4>', 850, 550)
        assert shell.scroll_position == 0
        event('<ButtonPress-1>', 850, 605)
        event('<B1-Motion>', 850, 570)
        event('<ButtonRelease-1>', 850, 570)
        assert shell.scroll_position == 1, 'Touch drag scrolling failed'
        shell.root.event_generate('<F3>')
        pump()
        assert shell.page == 'device'
        shell.root.event_generate('<Escape>')
        pump()
        assert shell.page == 'workspace'
        print(json.dumps({'ok': True, 'ui_uid': os.getuid(), 'keyboard_navigation': True,
                          'tap_action_actual_libinput_and_disk': True, 'drag_cancel': True,
                          'drawing_survives_poll': True, 'scroll': True, 'touch_drag_scroll': True,
                          'device_navigation': True}))
    finally:
        request('input-settings', original)
        shell.root.destroy()


def main():
    if '--client' in sys.argv:
        client()
        return
    auth = Path('/run/companion-desktop/input-test-Xauthority')
    auth.touch()
    subprocess.run(['xauth', '-f', str(auth), 'add', ':99', '.', secrets.token_hex(16)], check=True)
    os.chown(auth, 0, pwd.getpwnam('companion-ui').pw_gid)
    os.chmod(auth, 0o640)
    log = Path('/var/lib/companion/desktop-stage/evidence/input-xvfb.log')
    server = subprocess.Popen(['Xvfb', ':99', '-screen', '0', '1280x800x24', '-nolisten', 'tcp', '-auth', str(auth)],
                              stdout=log.open('w'), stderr=subprocess.STDOUT)
    try:
        time.sleep(0.5)
        result = subprocess.run(['su', '-s', '/bin/sh', '-c',
                                 f'DISPLAY=:99 XAUTHORITY={auth} exec python3 {Path(__file__).resolve()} --client',
                                 'companion-ui'], capture_output=True, text=True, timeout=40)
        print(result.stdout, end='')
        print(result.stderr, file=sys.stderr, end='')
        assert result.returncode == 0, 'Input interaction test failed'
        Path('/var/lib/companion/desktop-stage/evidence/input-interaction.json').write_text(result.stdout)
    finally:
        server.terminate()
        server.wait(timeout=5)
        auth.unlink(missing_ok=True)


if __name__ == '__main__':
    main()

"""Real user/Tk/application tests on an isolated X display; no reboot or boot writes."""
import json
import os
from pathlib import Path
import pwd
import secrets
import subprocess
import sys
import tempfile
import time
from unittest.mock import patch

sys.path.insert(0, '/opt/companion/desktop')


def client():
    import applications as apps
    from shell import Shell, request
    virtual = request().get('platform') == 'hyperv-dev'
    fixture = tempfile.TemporaryDirectory(prefix='companion-workspace-')
    base = Path(fixture.name)
    apps.HOME = base
    apps.DOCUMENTS = base/'Documents'
    apps.STATE = base/'state'
    shell = Shell()
    manager = shell.apps
    wm = subprocess.Popen(['openbox', '--config-file', '/opt/companion/desktop/openbox.xml'],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    def pump(seconds=0.2):
        end = time.monotonic()+seconds
        while time.monotonic() < end:
            shell.root.update()
            time.sleep(0.02)
    def select(files, path):
        key = next(key for key, item in files.entries.items() if item == path)
        files.tree.selection_set(key)
    def wait_window(name):
        for _ in range(40):
            pump(0.25)
            found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', name],
                                   capture_output=True, text=True)
            if found.stdout.split():
                return found.stdout.split()[-1]
        raise AssertionError('Application window did not appear: '+name)
    results = {'ui_uid': os.getuid(), 'boot_id': request()['boot_id'], 'virtual_hardware': virtual}
    try:
        for _ in range(40):
            pump(0.25)
            if shell.data:
                break
        assert shell.data
        if virtual:
            home_text = [shell.canvas.itemcget(item, 'text') for item in shell.canvas.find_all()
                         if shell.canvas.type(item) == 'text']
            assert 'Your desktop' in home_text
            assert 'Virtual audio · No speakers' in home_text
            assert shell.panel.winfo_viewable()
            shell.show('overview')
            overview_text = [shell.canvas.itemcget(item, 'text') for item in shell.canvas.find_all()
                             if shell.canvas.type(item) == 'text']
            assert 'Power source unavailable' in overview_text
            shell.show('workspace')
            results['vm_home_capabilities_and_power_state'] = True
        editor = manager.editor()
        path = apps.DOCUMENTS/'workspace-test.txt'
        editor.text.insert('1.0', 'Companion document\n')
        pump()
        assert editor.dirty
        assert editor.save(path) and path.read_text() == 'Companion document\n'
        os.chmod(path, 0o600)
        editor.text.insert('end', 'Saved again.\n')
        pump()
        assert editor.save() and path.stat().st_mode & 0o777 == 0o600
        editor.open(path)
        assert editor.text.get('1.0', 'end-1c') == path.read_text()
        editor.text.insert('end', 'Recovered draft.\n')
        pump(2)
        draft_text = editor.text.get('1.0', 'end-1c')
        assert json.loads(editor.draft.read_text())['text'] == draft_text
        editor.win.destroy()  # Simulate losing this document window without a save/close.
        with patch.object(apps.messagebox, 'askyesno', return_value=True):
            recovered = manager.editor()
        assert recovered.dirty and recovered.text.get('1.0', 'end-1c') == draft_text
        assert recovered.save() and path.read_text() == draft_text
        recovered.close()
        results['editor_write_read_private_mode_and_draft_recovery'] = True
        files = manager.launch('files')
        pump(1)
        assert any('Files' in child.cget('text') for child in shell.panel_tasks.winfo_children())
        select(files, path)
        renamed = path.with_name('renamed.txt')
        with patch.object(apps.simpledialog, 'askstring', return_value=renamed.name):
            files.rename()
        assert renamed.exists() and not path.exists()
        select(files, renamed)
        with patch.object(apps.messagebox, 'askyesno', return_value=True):
            files.trash()
        assert not renamed.exists()
        files.show_trash()
        trashed = next(iter(files.entries.values()))
        select(files, trashed)
        files.restore()
        assert renamed.read_text() == draft_text and not trashed.exists()
        with patch.object(apps.simpledialog, 'askstring', return_value='New folder'):
            files.navigate(apps.DOCUMENTS)
            files.mkdir()
        assert (apps.DOCUMENTS/'New folder').is_dir()
        results['files_create_rename_trash_restore'] = True
        files.close()
        terminal = manager.launch('terminal')
        window = wait_window('^Companion Terminal$')
        report = base/'terminal-uid.txt'
        subprocess.run(['xdotool', 'windowactivate', '--sync', window], check=True)
        pump(0.6)  # Mapping precedes the PTY being ready for typed commands.
        subprocess.run(['xdotool', 'windowactivate', '--sync', window, 'type', '--clearmodifiers',
                        f'id -u > {report}'], check=True)
        subprocess.run(['xdotool', 'key', 'Return'], check=True)
        pump(1)
        assert report.read_text().strip() == '1000'
        subprocess.run(['xdotool', 'windowminimize', window], check=True)
        manager.launch('terminal')
        pump()
        assert subprocess.check_output(['xdotool', 'getactivewindow']).decode().strip() == window
        switcher = manager.switcher()
        pump()
        assert window in [str(int(key, 16)) for key in switcher.tree.get_children()]
        switcher.close()
        results['terminal_unprivileged_minimize_restore_and_switcher'] = True
        manager.launch('browser')
        wait_window('Companion.*Mozilla Firefox')
        results['firefox_local_start_page'] = True
        sound = manager.launch('sound')
        if virtual:
            assert 'no physical speaker output' in sound.message.get()
        original = subprocess.check_output(['pactl', 'get-sink-volume', '@DEFAULT_SINK@'], text=True)
        import re
        volume = int(re.search(r'(\d+)%', original)[1])
        mute = subprocess.check_output(['pactl', 'get-sink-mute', '@DEFAULT_SINK@'], text=True).strip().endswith('yes')
        try:
            sound.volume.set(25)
            sound.apply()
            assert '25%' in subprocess.check_output(['pactl', 'get-sink-volume', '@DEFAULT_SINK@'], text=True)
            sound.mute()
            changed = subprocess.check_output(['pactl', 'get-sink-mute', '@DEFAULT_SINK@'], text=True).strip().endswith('yes')
            assert changed != mute
        finally:
            subprocess.run(['pactl', 'set-sink-volume', '@DEFAULT_SINK@', str(volume)+'%'], check=True)
            subprocess.run(['pactl', 'set-sink-mute', '@DEFAULT_SINK@', '1' if mute else '0'], check=True)
        sound.close()
        results['audio_sink_volume_mute_readback_restore'] = True
        firmware = manager.launch('firmware')
        for _ in range(40):
            pump(0.25)
            if firmware.attributes or (virtual and 'unavailable' in firmware.message.get()):
                break
        if virtual:
            assert not firmware.attributes and 'unavailable' in firmware.message.get()
        else:
            assert len(firmware.attributes) > 20
            assert firmware.attributes['KeyboardIllumination']['writable']
            assert not firmware.attributes['SecureBoot']['writable']
        firmware.close()
        results['bios_supported_attributes_and_write_allowlist'] = 'unavailable in VM' if virtual else True
        network = manager.launch('network')
        if virtual:
            assert not network.wifi_available
            assert 'no Wi-Fi adapter' in network.message.get()
            assert not hasattr(network, 'password')
        else:
            for _ in range(60):
                pump(0.25)
                if 'Scanning' not in network.message.get():
                    break
            assert 'networks on' in network.message.get(), network.message.get()
        network.close()
        results['actual_wifi_scan_ethernet_unchanged'] = 'no virtual Wi-Fi; Ethernet retained' if virtual else True
        results['ok'] = True
        print(json.dumps(results), flush=True)
    finally:
        for process in manager.processes.values():
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
        wm.terminate()
        wm.wait(timeout=5)
        shell.root.destroy()
        fixture.cleanup()


def main():
    if '--client' in sys.argv:
        client()
        return
    out = Path('/var/lib/companion/desktop-stage/evidence')
    auth = Path('/run/companion-desktop/workspace-test-Xauthority')
    auth.touch()
    subprocess.run(['xauth', '-f', str(auth), 'add', ':99', '.', secrets.token_hex(16)], check=True)
    os.chown(auth, 0, pwd.getpwnam('companion-ui').pw_gid)
    os.chmod(auth, 0o640)
    server = subprocess.Popen(['Xvfb', ':99', '-screen', '0', '1280x800x24', '-nolisten', 'tcp', '-auth', str(auth)],
                              stdout=(out/'workspace-xvfb.log').open('w'), stderr=subprocess.STDOUT)
    try:
        time.sleep(0.5)
        result = subprocess.run(['su', '-s', '/bin/sh', '-c',
            f'DISPLAY=:99 XAUTHORITY={auth} XDG_RUNTIME_DIR=/run/companion-desktop/user '
            f'exec dbus-run-session python3 {Path(__file__).resolve()} --client', 'companion-ui'],
            capture_output=True, text=True, timeout=100)
        (out/'workspace-test.log').write_text(result.stdout+result.stderr)
        print(result.stdout, end='')
        if result.returncode:
            print(result.stderr, file=sys.stderr, end='')
        assert result.returncode == 0, 'Workspace test failed'
        (out/'workspace.json').write_text(result.stdout)
    finally:
        server.terminate()
        server.wait(timeout=5)
        auth.unlink(missing_ok=True)


if __name__ == '__main__':
    main()

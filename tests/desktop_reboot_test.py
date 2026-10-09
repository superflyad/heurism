"""Deliberate normal API reboot and postboot evidence, preserving SSD recovery."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, '/opt/companion/desktop')
from shell import request
import release
import system_actions

OUT = Path('/var/lib/companion/desktop-stage/evidence')


def main():
    if '--seed' in sys.argv:
        assert os.getuid() == 1000
        before = request()
        path = Path.home()/'Documents'/('Companion verification '+before['boot_id']+'.txt')
        with path.open('x') as stream:
            stream.write('Companion workspace verification\nThis document survived a normal reboot.\n')
        print(path)
        return
    if '--start' in sys.argv:
        release.verify(release.LINK)
        with contextlib.redirect_stdout(io.StringIO()):
            release.health()
        system_actions.verify_boot()
        before = request()
        assert before['management'] == {'ssh': True, 'watch': True, 'boot_healthy': True}
        document = subprocess.check_output(['su', '-s', '/bin/sh', '-c',
            f'python3 {Path(__file__).resolve()} --seed', 'companion-ui'], text=True).strip()
        before['document'] = document
        before['document_text'] = Path(document).read_text()
        before['release'] = str(release.LINK.resolve())
        (OUT/'workspace-before-reboot.json').write_text(json.dumps(before, indent=2))
        print(json.dumps(request('power', {'operation': 'reboot', 'confirm': True})), flush=True)
        return
    if '--finish' not in sys.argv:
        raise ValueError('Choose --start or --finish explicitly')
    before = json.loads((OUT/'workspace-before-reboot.json').read_text())
    for _ in range(40):
        try:
            after = request()
            with contextlib.redirect_stdout(io.StringIO()):
                release.health()
            if after['management'] == {'ssh': True, 'watch': True, 'boot_healthy': True}:
                break
        except (OSError, ValueError):
            pass
        time.sleep(1)
    assert after['boot_id'] != before['boot_id']
    assert after['management'] == {'ssh': True, 'watch': True, 'boot_healthy': True}
    release.verify(release.LINK)
    assert str(release.LINK.resolve()) == before['release']
    assert after['preferences'] == before['preferences']
    assert Path(before['document']).read_text() == before['document_text']
    assert Path(before['document']).stat().st_uid == 1000
    boot = system_actions.run(['efibootmgr', '-v'])
    (OUT/'reboot-boot-before-normalization.txt').write_text(boot)
    system_actions.verify_boot()
    assert Path('/sys/class/tty/tty0/active').read_text().strip() == 'tty7'
    for service in ('companion-control', 'companion-desktop', 'sshd', 'companion-watch'):
        assert 'status: started' in system_actions.run(['rc-service', service, 'status'])
    effective = request('input-status')
    virtual = after.get('platform') == 'hyperv-dev'
    if virtual:
        assert effective is None, 'VM should not invent physical touchpad properties'
    else:
        for name, value in after['preferences']['input'].items():
            assert effective[name] == value
    sink = subprocess.check_output(['su', '-s', '/bin/sh', '-c',
        'XDG_RUNTIME_DIR=/run/companion-desktop/user pactl get-default-sink', 'companion-ui'], text=True).strip()
    assert sink and (virtual or 'Speaker' in sink)
    (OUT/'reboot-state.json').write_text(json.dumps(after, indent=2))
    (OUT/'input-reboot.json').write_text(json.dumps({'ok': True, 'boot_id': after['boot_id'],
        'preferences': after['preferences']['input'], 'effective': effective}, indent=2))
    result = {'ok': True, 'previous_boot_id': before['boot_id'], 'boot_id': after['boot_id'],
              'release': before['release'], 'preferences_preserved': True, 'document_preserved': True,
              'ui_ready_on_tty7': True, 'management': after['management'], 'audio_sink': sink,
              'virtual_hardware': virtual,
              'protected_hashes_and_boot_orders_verified': True}
    (OUT/'workspace-reboot.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

"""Fail an isolated cloned UI release and prove automatic physical fallback."""
import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

sys.path.insert(0, '/opt/companion/desktop')
from shell import request
import release
import system_actions


def main():
    current = release.verify(release.LINK)
    with contextlib.redirect_stdout(io.StringIO()):
        release.health()
    before = request()
    saved_previous = release.PREVIOUS.read_text()
    probe = release.ROOT/('failed-ui-probe-'+str(time.time_ns()))
    shutil.copytree(current, probe)
    # This clone passes integrity but fails execution. The working release and
    # its files remain intact; root SSH/watch/control are never stopped.
    (probe/'shell.py').write_text('raise RuntimeError("Public UI recovery test fixture")\n')
    release.seal(probe)
    release.verify(probe)
    link = release.LINK.with_name('desktop.test')
    assert not link.exists() and not link.is_symlink()
    try:
        release.PREVIOUS.write_text(str(current)+'\n')
        link.symlink_to(probe)
        link.replace(release.LINK)
        subprocess.run(['rc-service', 'companion-desktop', 'restart'], check=True, timeout=20)
        healthy = False
        for _ in range(40):
            time.sleep(1)
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    release.health()
                if release.LINK.resolve() == current:
                    healthy = True
                    break
            except (OSError, ValueError):
                pass
        assert healthy, 'Working UI did not return automatically'
        after = request()
        assert after['boot_id'] == before['boot_id']
        assert after['management'] == {'ssh': True, 'watch': True, 'boot_healthy': True}
        system_actions.verify_boot()
        result = {'ok': True, 'boot_id': after['boot_id'], 'working_release': str(current),
                  'failed_clone': str(probe), 'automatic_fallback_without_owner_input': True,
                  'management': after['management'], 'boot_files_and_orders_verified': True}
        filename = 'vm-rollback.json' if before.get('platform') == 'hyperv-dev' else 'physical-rollback.json'
        (Path('/var/lib/companion/desktop-stage/evidence')/filename).write_text(json.dumps(result, indent=2))
        print(json.dumps(result, indent=2))
    finally:
        if release.LINK.resolve() == probe:
            with contextlib.redirect_stdout(io.StringIO()):
                release.rollback()
            subprocess.run(['rc-service', 'companion-desktop', 'restart'], check=True, timeout=20)
        if release.LINK.resolve() == current:
            release.PREVIOUS.write_text(saved_previous)
        link.unlink(missing_ok=True)


if __name__ == '__main__':
    main()

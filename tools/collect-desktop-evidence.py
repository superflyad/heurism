"""Record live boot/service checks and fetch fixed proof files over pinned SSH."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
out = repo/'build/desktop'
command = ('cat /proc/sys/kernel/random/boot_id; rc-service companion-control status; '
           'rc-service companion-desktop status; rc-service sshd status; rc-service companion-watch status; '
           'cat /sys/class/tty/tty0/active; efibootmgr; efibootmgr --driver; '
           'sha256sum -c /var/lib/companion/presentation-20260927T161814Z/protected.sha256; '
           'sha256sum /opt/companion/desktop/control.py /opt/companion/desktop/shell.py '
           '/opt/companion/desktop/session-config.py /opt/companion/desktop/session.sh '
           '/opt/companion/desktop/client.sh /etc/init.d/companion-control /etc/init.d/companion-desktop')
command += '; sha256sum /opt/companion/desktop/input_settings.py'
extras = ('applications.py', 'system_actions.py', 'home.py', 'user-session.sh', 'release.py', 'openbox.xml', 'start.html')
command += '; sha256sum '+' '.join('/opt/companion/desktop/'+name for name in extras)
command += '; python3 /usr/local/sbin/companion-release verify; python3 /usr/local/sbin/companion-release health'
command += '; sha256sum /usr/local/sbin/companion-release'
result = subprocess.run([sys.executable, str(repo/'tools/dell.py'), '--command', command],
                        capture_output=True, text=True, check=True)
(out/'post-reboot.txt').write_text(result.stdout+result.stderr)
assert result.stdout.count('status: started') == 4 and result.stdout.count(': OK') == 6
assert 'BootOrder: 0005,0000\n' in result.stdout and 'DriverOrder: 0000,0001\n' in result.stdout
assert 'BootNext:' not in result.stdout and 'tty7\n' in result.stdout
mapping = {'control.py': 'control.py', 'shell.py': 'shell.py', 'session-config.py': 'session-config.py',
           'session.sh': 'session.sh', 'client.sh': 'client.sh',
           'control.initd': 'companion-control', 'desktop.initd': 'companion-desktop', 'input_settings.py': 'input_settings.py'}
mapping.update({name: name for name in extras})
for local, remote in mapping.items():
    digest = hashlib.sha256((repo/'userspace'/local).read_bytes()).hexdigest()
    assert any(line.startswith(digest+' ') and line.endswith('/'+remote)
               for line in result.stdout.splitlines()), 'Installed source mismatch: '+local
helper_digest = hashlib.sha256((repo/'userspace/release.py').read_bytes()).hexdigest()
assert any(line.startswith(helper_digest+' ') and line.endswith('/usr/local/sbin/companion-release')
           for line in result.stdout.splitlines()), 'Stable release helper mismatch'
state = json.loads((repo/'artifacts/targets/dell.json').read_text())
identity = repo/'artifacts/ssh'
options = ['-i', str(identity/'companion_client_ed25519'), '-o', 'BatchMode=yes',
           '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
           '-o', 'UserKnownHostsFile='+str(identity/'known_hosts')]
for name in ('integration.json', 'physical-check.json', 'reboot-state.json',
             'input-interaction.json', 'input-reboot.json', 'workspace.json', 'physical-apps.json',
             'physical-rollback.json', 'workspace-reboot.json', 'audio-playback.json', 'reboot-boot-before-normalization.txt'):
    subprocess.run(['scp', *options, 'root@'+state['address']+
                    ':/var/lib/companion/desktop-stage/evidence/'+name, str(out/name)], check=True, timeout=30)
facts = json.loads((out/'reboot-state.json').read_text())
assert facts['management'] == {'ssh': True, 'watch': True, 'boot_healthy': True}
assert facts['boot_id'] == state['boot_id']
print('Installed sources, protected hashes, boot orders and healthy UI/management verified.')

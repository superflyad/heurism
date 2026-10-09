"""Install reversible Companion presentation changes without replacing EFI loaders.

Requires the current healthy installed Dell management environment. Does not
reboot, change boot entries or alter the recovery journal. Saves rollback files.
"""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

from boot_safety import validate_command

repo = Path(__file__).resolve().parents[1]
keys = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
target = 'root@' + state['address']
options = ['-i', str(keys / 'companion_client_ed25519'), '-o', 'BatchMode=yes',
           '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes',
           '-o', 'HostKeyAlias=companion-dell', '-o',
           'UserKnownHostsFile=' + str(keys / 'known_hosts')]


def remote(command):
    validate_command(command)
    result = subprocess.run(['ssh'] + options + [target, command],
                            capture_output=True, text=True, timeout=30,
                            creationflags=0x08000000)
    if result.returncode:
        raise SystemExit('Remote operation failed:\n' + result.stdout + result.stderr)
    return result.stdout


preflight = remote('''set -eu
test "$(cat /etc/hostname)" = companion-dell
test "$(id -u)" = 0
test "$(findmnt -n -o SOURCE /)" = /dev/nvme0n1p2
test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1
test "$(cat /proc/sys/kernel/random/boot_id)" = "$(cat /var/lib/companion/healthy-boot-id)"
sshd -t
rc-service sshd status
rc-service companion-watch status
test "$(efibootmgr | sed -n 's/^BootOrder: //p')" = 0005,0000
test "$(efibootmgr --driver | sed -n 's/^DriverOrder: //p')" = 0000,0001
! efibootmgr | grep -q '^BootNext:'
test "$(grub-editenv /boot/efi/companion/recovery/grubenv list)" = companion_pending=0
sha256sum /boot/efi/EFI/alpine/grubx64.efi /boot/efi/EFI/boot/bootx64.efi /boot/efi/EFI/companion/recoveryx64.efi /boot/efi/EFI/companion/companionextx64.efi /boot/efi/companion/stable/vmlinuz-lts /boot/efi/companion/stable/initramfs-lts
cat /proc/sys/kernel/random/boot_id
''')
expected_loader = '5bc0e512af43def3ad39ce90b5084f1e56c73abc98d520e01466b7a7c7724efc'
for path in ['alpine/grubx64.efi', 'boot/bootx64.efi', 'companion/recoveryx64.efi']:
    assert expected_loader + '  /boot/efi/EFI/' + path in preflight
assert '2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4  /boot/efi/EFI/companion/companionextx64.efi' in preflight
tag = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
stage = '/var/lib/companion/presentation-' + tag
remote('mkdir -p ' + shlex.quote(stage))
sources = ['recovery-grub.cfg', 'companion-welcome', 'welcome.initd']
digests = {}
for name in sources:
    local = repo / 'provisioning' / name
    digests[name] = hashlib.sha256(local.read_bytes()).hexdigest()
    subprocess.run(['scp'] + options + [str(local), target + ':' + stage + '/' + name],
                   check=True, timeout=20, creationflags=0x08000000)
checks = '\n'.join('test "$(sha256sum ' + name + " | cut -d ' ' -f 1)\" = " + shlex.quote(digest)
                   for name, digest in digests.items())
result = remote('''set -eu
cd ''' + shlex.quote(stage) + '\n' + checks + '''
sh -n companion-welcome
sh -n welcome.initd
uuid=$(findmnt -n -o UUID /)
test -n "$uuid"
sed "s/@ROOT_UUID@/$uuid/g" recovery-grub.cfg > grub.cfg.new
grub-script-check grub.cfg.new
cp -p /boot/efi/companion/recovery/grub.cfg grub.cfg.before
cp -p /etc/issue issue.before
cp -p /etc/motd motd.before
for file in /etc/companion/bin/companion-welcome /etc/init.d/companion-welcome; do
    if [ -f "$file" ]; then cp -p "$file" "$(basename "$(dirname "$file")").before"; fi
done
rc-update show default > runlevel.before
sha256sum /boot/efi/EFI/alpine/grubx64.efi /boot/efi/EFI/boot/bootx64.efi /boot/efi/EFI/companion/recoveryx64.efi /boot/efi/EFI/companion/companionextx64.efi /boot/efi/companion/stable/vmlinuz-lts /boot/efi/companion/stable/initramfs-lts > protected.sha256
cp companion-welcome /etc/companion/bin/companion-welcome.new
chmod 755 /etc/companion/bin/companion-welcome.new
mv /etc/companion/bin/companion-welcome.new /etc/companion/bin/companion-welcome
ln -sf /etc/companion/bin/companion-welcome /usr/local/sbin/companion-welcome
cp welcome.initd /etc/init.d/companion-welcome
chmod 755 /etc/init.d/companion-welcome
rc-update add companion-welcome default
cp grub.cfg.new /boot/efi/companion/recovery/grub.cfg.new
mv /boot/efi/companion/recovery/grub.cfg.new /boot/efi/companion/recovery/grub.cfg
/etc/companion/bin/companion-welcome --show
sha256sum -c protected.sha256
test "$(grub-editenv /boot/efi/companion/recovery/grubenv list)" = companion_pending=0
sshd -t
rc-service sshd status
rc-service companion-watch status
sync
sha256sum /boot/efi/companion/recovery/grub.cfg /etc/companion/bin/companion-welcome /etc/init.d/companion-welcome
cat /run/companion/welcome.txt
''')
evidence = repo / 'artifacts/startup'
evidence.mkdir(parents=True, exist_ok=True)
record = {'stage': stage, 'source_sha256': digests, 'preflight': preflight,
          'installation': result, 'rebooted': False}
(evidence / 'presentation-deployment.json').write_text(json.dumps(record, indent=2))
print(result)
print('Rollback files:', stage)

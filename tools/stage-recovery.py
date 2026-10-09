"""Stage rescue files on the authenticated Dell without changing boot order.

Keeps the current EFI loaders intact. Copies the known working main kernel and
initramfs onto EFI so loading those files does not depend on the root filesystem.
"""
import hashlib
from pathlib import Path
import subprocess
import tarfile

repo = Path(__file__).resolve().parents[1]
keys = repo / 'artifacts/ssh'
output = repo / 'build/recovery'
archive = output / 'rescue.tar'
with tarfile.open(archive, 'w') as packed:
    for file in sorted((output / 'payload').rglob('*')):
        if file.is_file() and file.relative_to(output / 'payload').parts[0] not in ('EFI', 'companion'):
            packed.add(file, arcname=file.relative_to(output / 'payload').as_posix())
options = ['-i', str(keys / 'companion_client_ed25519'), '-o', 'BatchMode=yes',
           '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
           '-o', 'UserKnownHostsFile=' + str(keys / 'known_hosts')]
target = 'root@10.8.22.238'
def run(command):
    subprocess.run(['ssh'] + options + [target, command], check=True, creationflags=0x08000000)
run('mkdir -p /var/lib/companion/recovery-stage')
for path in [archive, repo / 'provisioning/recovery-grub.cfg',
             repo / 'provisioning/companion-boot-health', repo / 'provisioning/boot-health.initd',
             repo / 'provisioning/companion-next-boot', repo / 'provisioning/recovery-sysctl.conf',
             repo / 'provisioning/companion-recovery-deadline', repo / 'provisioning/recovery-deadline.initd',
             keys / 'companion_client_ed25519.pub', keys / 'companion_target_ed25519.pub']:
    subprocess.run(['scp'] + options + [str(path), target + ':/var/lib/companion/recovery-stage/'],
                   check=True, creationflags=0x08000000)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
run("""set -eu
cd /var/lib/companion/recovery-stage
test "$(sha256sum rescue.tar | cut -d ' ' -f 1)" = '""" + digest + """'
test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1
test "$(cat /sys/block/nvme0n1/device/serial | tr -d ' ')" = PHTE040301J9512B-1
free=$(df -k /boot/efi | tail -1 | awk '{print $4}')
replaced=$(du -sk /boot/efi/boot /boot/efi/apks 2>/dev/null | awk '{sum+=$1} END {print sum+0}')
test "$((free + replaced))" -gt 440000
mkdir -p /var/lib/companion/efi-backup
if [ ! -d /var/lib/companion/efi-backup/EFI ]; then
    cp -a /boot/efi/EFI /var/lib/companion/efi-backup/
    efibootmgr -v > /var/lib/companion/efi-backup/boot-entries-before.txt
fi
tar -xf rescue.tar -C /boot/efi
if [ -f /boot/efi/.boot_repository ] && [ ! -s /boot/efi/.boot_repository ]; then
    rm /boot/efi/.boot_repository
fi
mkdir -p /boot/efi/companion/stable /boot/efi/companion/recovery /boot/efi/EFI/companion
cp /boot/vmlinuz-lts /boot/initramfs-lts /boot/efi/companion/stable/
uuid=$(findmnt -n -o UUID /)
sed "s/@ROOT_UUID@/$uuid/g" recovery-grub.cfg > /boot/efi/companion/recovery/grub.cfg
if [ ! -f /boot/efi/companion/recovery/grubenv ]; then
    grub-editenv /boot/efi/companion/recovery/grubenv create
    grub-editenv /boot/efi/companion/recovery/grubenv set companion_pending=1
fi
printf '%s\\n' 'insmod part_gpt' 'insmod part_msdos' 'insmod fat' \\
    'search --no-floppy --file --set=root /companion/recovery/grub.cfg' \\
    'configfile /companion/recovery/grub.cfg' > embedded.cfg
grub-mkstandalone -O x86_64-efi --locales='' --fonts='' \\
    --modules='part_gpt part_msdos fat search search_fs_file normal linux loadenv gzio' \\
    -o /boot/efi/EFI/companion/recoveryx64.efi 'boot/grub/grub.cfg=embedded.cfg'
cp companion-boot-health /etc/companion/bin/companion-boot-health
cp companion_client_ed25519.pub /etc/companion/client-access.pub
cp companion_target_ed25519.pub /etc/companion/host-identity.pub
chmod 755 /etc/companion/bin/companion-boot-health
cp companion-next-boot /etc/companion/bin/companion-next-boot
chmod 755 /etc/companion/bin/companion-next-boot
ln -sf /etc/companion/bin/companion-next-boot /usr/local/sbin/companion-next-boot
mkdir -p /etc/sysctl.d
cp recovery-sysctl.conf /etc/sysctl.d/90-companion-recovery.conf
cp boot-health.initd /etc/init.d/companion-boot-health
chmod 755 /etc/init.d/companion-boot-health
rc-update add companion-boot-health default
cp companion-recovery-deadline /etc/companion/bin/companion-recovery-deadline
cp recovery-deadline.initd /etc/init.d/companion-recovery-deadline
chmod 755 /etc/companion/bin/companion-recovery-deadline /etc/init.d/companion-recovery-deadline
rc-update add companion-recovery-deadline boot
rc-update add sshd default
sync
df -m /boot/efi
sha256sum /boot/efi/EFI/companion/recoveryx64.efi
echo 'STAGED: original boot entries and loaders unchanged; recovery journal preserved'
""")
# Retrieve the exact standalone EFI loader and journal to exercise in QEMU.
for remote, local in [('EFI/companion/recoveryx64.efi', 'EFI/BOOT/BOOTX64.EFI'),
                      ('companion/recovery/grub.cfg', 'companion/recovery/grub.cfg'),
                      ('companion/recovery/grubenv', 'companion/recovery/grubenv')]:
    destination = output / 'payload' / local
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['scp'] + options + [target + ':/boot/efi/' + remote, str(destination)],
                   check=True, creationflags=0x08000000)
print('Staged rescue and retrieved the exact EFI loader for virtual testing.')

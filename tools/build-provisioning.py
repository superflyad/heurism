"""Assemble a custom Alpine diskless provisioning payload with SSH identities.

Only the target's host private key and this workspace's authorized PUBLIC key
go on the USB. The client private key stays in artifacts/ssh on this computer.
"""
import gzip
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

repo = Path(__file__).resolve().parents[1]
alpine = repo / 'build' / 'provisioning' / 'alpine'
payload = repo / 'build' / 'provisioning' / 'payload'
identities = repo / 'artifacts' / 'ssh'
identities.mkdir(parents=True, exist_ok=True)

def key(name):
    path = identities / name
    if not path.exists():
        subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-C', name, '-f', str(path)], check=True)
    return path

client = key('companion_client_ed25519')
host = key('companion_target_ed25519')
world = ['alpine-base', 'openssh', 'openssh-server-common-openrc',
         'wpa_supplicant', 'iw', 'pciutils', 'usbutils', 'efibootmgr', 'util-linux',
         'parted', 'dosfstools', 'e2fsprogs', 'grub-efi',
         'linux-firmware-intel', 'linux-firmware-i915', 'linux-firmware-rtl_nic']
apkdir = alpine / 'apks' / 'x86_64'
for package in world:
    assert list(apkdir.glob(package + '-[0-9]*.apk')), f'Offline package missing: {package}'

payload.mkdir(parents=True, exist_ok=True)
for directory in ['boot', 'apks', 'efi']:
    shutil.copytree(alpine / directory, payload / directory, dirs_exist_ok=True)
shutil.copyfile(alpine / '.alpine-release', payload / '.alpine-release')
companion = repo / 'build' / 'esp' / 'EFI' / 'BOOT' / 'BOOTX64.EFI'
(payload / 'efi' / 'companion').mkdir(exist_ok=True)
shutil.copyfile(companion, payload / 'efi' / 'companion' / 'Companion.efi')
grub = '''set timeout=5
set default=0
menuentry "Companion provisioning - remote access and installer" {
    linux /boot/vmlinuz-lts modules=loop,squashfs,sd-mod,usb-storage console=ttyS0,115200n8 console=tty1
    initrd /boot/intel-ucode.img /boot/amd-ucode.img /boot/initramfs-lts
}
menuentry "Companion original UEFI framebuffer experiment" {
    chainloader /efi/companion/Companion.efi
}
menuentry "UEFI firmware settings" { fwsetup }
'''
(payload / 'boot' / 'grub' / 'grub.cfg').write_text(grub, encoding='ascii', newline='\n')
files = {}
def add(name, value, mode=0o644):
    if isinstance(value, str):
        value = value.replace('\r\n', '\n').encode()
    files[name] = (value, mode)

add('etc/.default_boot_services', '')
add('etc/hostname', 'companion-dell\n')
add('etc/apk/world', '\n'.join(world) + '\n')
add('etc/network/interfaces', 'auto lo\niface lo inet loopback\n')
add('etc/ssh/sshd_config', '''Port 22
PermitRootLogin prohibit-password
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
AuthorizedKeysFile .ssh/authorized_keys
HostKey /etc/ssh/ssh_host_ed25519_key
Subsystem sftp /usr/lib/ssh/sftp-server
''')
add('root/.ssh/authorized_keys', client.with_suffix('.pub').read_bytes(), 0o600)
add('etc/ssh/ssh_host_ed25519_key', host.read_bytes(), 0o600)
add('etc/ssh/ssh_host_ed25519_key.pub', host.with_suffix('.pub').read_bytes())
add('etc/companion/host-identity.pub', host.with_suffix('.pub').read_bytes())
add('etc/companion/client-access.pub', client.with_suffix('.pub').read_bytes())
add('etc/lbu/include', '/root/.ssh\n/usr/local/sbin\n')
add('etc/profile.d/companion.sh', 'export PATH=/etc/companion/bin:$PATH\n')
for script in sorted((repo / 'provisioning').glob('companion-*')):
    add('etc/companion/bin/' + script.name, script.read_text(), 0o755)
add('etc/init.d/companion', (repo / 'provisioning' / 'companion.initd').read_text(), 0o755)
add('etc/init.d/companion-watch', (repo / 'provisioning' / 'network-watch.initd').read_text(), 0o755)
add('etc/init.d/companion-boot-health', (repo / 'provisioning' / 'boot-health.initd').read_text(), 0o755)
add('etc/init.d/companion-recovery-deadline', (repo / 'provisioning' / 'recovery-deadline.initd').read_text(), 0o755)
add('etc/init.d/companion-welcome', (repo / 'provisioning' / 'welcome.initd').read_text(), 0o755)
add('etc/sysctl.d/90-companion-recovery.conf', (repo / 'provisioning' / 'recovery-sysctl.conf').read_text())

overlay = payload / 'companion.apkovl.tar.gz'
with overlay.open('wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0) as compressed:
    with tarfile.open(fileobj=compressed, mode='w') as archive:
        directories = set()
        for name in files:
            parent = Path(name).parent
            while str(parent) != '.':
                directories.add(parent.as_posix()); parent = parent.parent
        directories.add('etc/runlevels/default')
        for name in sorted(directories):
            item = tarfile.TarInfo(name); item.type = tarfile.DIRTYPE
            item.mode = 0o700 if name.startswith('root') else 0o755
            archive.addfile(item)
        for name, (value, mode) in sorted(files.items()):
            item = tarfile.TarInfo(name); item.mode = mode; item.size = len(value)
            archive.addfile(item, io.BytesIO(value))
        for service in ('companion', 'companion-watch', 'companion-welcome'):
            item = tarfile.TarInfo('etc/runlevels/default/' + service)
            item.type = tarfile.SYMTYPE; item.mode = 0o777; item.linkname = '/etc/init.d/' + service
            archive.addfile(item)

host_public = host.with_suffix('.pub').read_text().strip().split()
(identities / 'known_hosts').write_text('companion-dell ' + ' '.join(host_public[:2]) + '\n')
manifest = {'base': 'Alpine Linux 3.24.2 extended x86_64',
            'purpose': 'temporary provisioning and persistent Linux management installation',
            'client_public_key': client.with_suffix('.pub').read_text().strip(),
            'target_host_public_key': host.with_suffix('.pub').read_text().strip(),
            'files': {p.relative_to(payload).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in sorted(payload.rglob('*'))
                      if p.is_file() and p.name != 'companion-manifest.json'}}
(payload / 'companion-manifest.json').write_text(json.dumps(manifest, indent=2))
print(f'Provisioning payload: {payload}')
print(f'Client private key retained locally: {client}')
print(f'Payload bytes: {sum(p.stat().st_size for p in payload.rglob("*") if p.is_file())}')

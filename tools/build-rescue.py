"""Build a small, offline Ethernet rescue system for the internal EFI partition.

Uses the verified Alpine ISO, its signed package index, and existing SSH identity.
Does not write a physical disk. APK dependency resolution is verified by booting.
"""
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import tarfile

repo = Path(__file__).resolve().parents[1]
base = repo / 'build/provisioning/alpine'
out = repo / 'build/recovery/payload'
apkdir = base / 'apks/x86_64'
world = ['alpine-base', 'openssh', 'openssh-server-common-openrc', 'openssl',
         'util-linux', 'pciutils', 'usbutils', 'efibootmgr', 'dosfstools',
         'e2fsprogs', 'grub-efi']
with tarfile.open(apkdir / 'APKINDEX.tar.gz') as archive:
    records = archive.extractfile('APKINDEX').read().decode().strip().split('\n\n')
packages = {}
providers = {}
def name(dependency):
    return re.split('[<>=~]', dependency, maxsplit=1)[0]
for record in records:
    fields = dict(line.split(':', 1) for line in record.splitlines() if ':' in line)
    packages[fields['P']] = fields
    for provided in [fields['P']] + fields.get('p', '').split():
        providers.setdefault(name(provided), []).append(fields['P'])
selected = set()
queue = list(world)
while True:
    while queue:
        requirement = queue.pop()
        if requirement.startswith('!'):
            continue
        wanted = name(requirement)
        choices = providers.get(wanted, [])
        assert choices, f'Missing dependency: {requirement}'
        package = wanted if wanted in choices else sorted(choices)[0]
        if package in selected:
            continue
        selected.add(package)
        queue.extend(packages[package].get('D', '').split())
    # APK also installs optional subpackages when all install_if conditions
    # match, e.g. util-linux's actual mount/findmnt programs and busybox TLS.
    provided = {name(value) for package in selected
                for value in [package] + packages[package].get('p', '').split()}
    for package, fields in packages.items():
        conditions = fields.get('i', '').split()
        if package not in selected and conditions and all(
                (name(c[1:]) not in provided) if c.startswith('!') else (name(c) in provided)
                for c in conditions):
            queue.append(package)
    if not queue:
        break
out.mkdir(parents=True, exist_ok=True)
(out / 'boot').mkdir(exist_ok=True)
(out / 'apks/x86_64').mkdir(parents=True, exist_ok=True)
for file in (out / 'apks/x86_64').glob('*.apk'):
    file.unlink()  # Only generated cache files in this exact output directory.
for package in sorted(selected):
    fields = packages[package]
    source = apkdir / f"{package}-{fields['V']}.apk"
    assert source.is_file(), source
    shutil.copyfile(source, out / 'apks/x86_64' / source.name)
shutil.copyfile(apkdir / 'APKINDEX.tar.gz', out / 'apks/x86_64/APKINDEX.tar.gz')
for filename in ['vmlinuz-lts', 'initramfs-lts', 'modloop-lts', 'intel-ucode.img']:
    shutil.copyfile(base / 'boot' / filename, out / 'boot' / filename)
shutil.copyfile(base / '.alpine-release', out / '.alpine-release')
# The marker belongs in apks/, exactly as on the verified Alpine ISO.
if (out / '.boot_repository').exists():
    (out / '.boot_repository').unlink()
shutil.copyfile(base / 'apks/.boot_repository', out / 'apks/.boot_repository')
files = {}
with tarfile.open(repo / 'build/provisioning/payload/companion.apkovl.tar.gz') as source:
    for item in source:
        if item.isfile():
            files[item.name] = (source.extractfile(item).read(), item.mode)
def add(path, data, mode=0o644):
    files[path] = (data.replace('\r\n', '\n').encode() if isinstance(data, str) else data, mode)
add('etc/apk/world', '\n'.join(world) + '\n')
add('etc/companion/rescue', 'Internal EFI rescue: RAM root; main root not mounted automatically.\n')
add('etc/companion/bin/companion-next-boot', (repo / 'provisioning/companion-next-boot').read_text(), 0o755)
add('etc/companion/bin/companion-watch', (repo / 'provisioning/companion-watch').read_text(), 0o755)
add('etc/companion/host-identity.pub', (repo / 'artifacts/ssh/companion_target_ed25519.pub').read_bytes())
add('etc/companion/client-access.pub', (repo / 'artifacts/ssh/companion_client_ed25519.pub').read_bytes())
add('etc/companion/bin/companion-start', '''#!/bin/sh
set -eu
mkdir -p /run/companion /var/lib/companion /usr/local/sbin
sed -i '/^tty0::respawn:/d' /etc/inittab
for script in /etc/companion/bin/*; do
    ln -sf "$script" "/usr/local/sbin/${script##*/}"
done
modprobe vmd 2>/dev/null || true
modprobe r8152 2>/dev/null || true
modprobe ax88179_178a 2>/dev/null || true
for path in /sys/class/net/*; do
    interface=${path##*/}
    [ "$interface" != lo ] || continue
    [ ! -d "$path/wireless" ] || continue
    ip link set "$interface" up || true
    udhcpc -i "$interface" -b -t 5 -T 2 -x hostname:companion-dell \\
        -p "/run/companion/dhcp-$interface.pid" >"/run/companion/dhcp-$interface.log" 2>&1 &
done
rc-service sshd start
# Do not arm iTCO: its physical reset test failed on target 0.
echo 'COMPANION INTERNAL RESCUE: Ethernet root SSH; RAM filesystem.' > /etc/motd
echo 'The main installation is not mounted automatically.' >> /etc/motd
cat /etc/motd > /dev/console
echo COMPANION_RESCUE_READY > /dev/console
''', 0o755)
add('etc/conf.d/watchdog', 'WATCHDOG_DEV="/dev/watchdog0"\nWATCHDOG_OPTS="-t 10 -T 120"\n')
add('etc/sysctl.d/90-companion-recovery.conf', (repo / 'provisioning/recovery-sysctl.conf').read_text())
add('etc/companion/bin/companion-status', '''#!/bin/sh
cat /etc/motd
ip -4 -o addr show scope global
ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
findmnt /
''', 0o755)
overlay = out / 'companion-rescue.apkovl.tar.gz'
with overlay.open('wb') as raw, gzip.GzipFile(fileobj=raw, mode='wb', mtime=0) as zipped:
    with tarfile.open(fileobj=zipped, mode='w') as archive:
        directories = {'etc/runlevels/default'}
        for path in files:
            parent = Path(path).parent
            while str(parent) != '.':
                directories.add(parent.as_posix()); parent = parent.parent
        for path in sorted(directories):
            item = tarfile.TarInfo(path); item.type = tarfile.DIRTYPE
            item.mode = 0o700 if path.startswith('root') else 0o755
            archive.addfile(item)
        for path, (data, mode) in sorted(files.items()):
            item = tarfile.TarInfo(path); item.mode = mode; item.size = len(data)
            archive.addfile(item, io.BytesIO(data))
        for service in ['companion', 'companion-watch']:
            item = tarfile.TarInfo('etc/runlevels/default/' + service)
            item.type = tarfile.SYMTYPE; item.linkname = '/etc/init.d/' + service
            archive.addfile(item)
manifest = {p.relative_to(out).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(out.rglob('*')) if p.is_file() and p.name != 'manifest.json'
            and p.relative_to(out).parts[0] not in ('EFI', 'companion')}
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2))
total = sum(p.stat().st_size for p in out.rglob('*') if p.is_file())
assert total < 440 * 1024**2, 'Reserve space for stable boot files and EFI loaders'
print(f'Rescue: {len(selected)} packages, {total / 1024**2:.1f} MiB, {out}')

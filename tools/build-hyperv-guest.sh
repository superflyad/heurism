#!/bin/bash
# Run on the authenticated PrimeLinux builder, never on a physical target.
# Produces a new disk file; never partitions an existing host block device.
set -euo pipefail
stage=${1:?staging directory required}
case "$stage" in /var/lib/companion-vm-build/*) ;; *) echo 'Invalid build directory' >&2; exit 1;; esac
test "$(id -u)" = 0
test -f "$stage/native.tar.gz"
test -f "$stage/client.pub"
test "$(sha256sum "$stage/heurism-os-release" | cut -d ' ' -f 1)" = 4cb2272e2f85fe61c37fab906149fb8624035ef38847d5b5439ba8e1d7bbb367
test "$(sha256sum "$stage/heurism-upstream-release" | cut -d ' ' -f 1)" = 1eb5561b4eae9962ff0f16ba7900cdc1f444535490f4863954e5bceab26beab3
test "$(sha256sum "$stage/heurism-wallpaper.svg" | cut -d ' ' -f 1)" = d65fab95b78ecacf6f744d08c43048b5b56fb62007fc148796f4b8cc878ca8fb
test "$(sha256sum "$stage/heurism-vm-packages.list" | cut -d ' ' -f 1)" = 9c1c677b5181e3c0b949aad41c0b8fa9049b77e6e343d84e4788415230d52270
test ! -e "$stage/guest.raw"
root="$stage/root"
mkdir -p "$root"
truncate -s 24G "$stage/guest.raw"
parted -s "$stage/guest.raw" mklabel gpt mkpart ESP fat32 1MiB 513MiB set 1 esp on mkpart root ext4 513MiB 100%
loop=$(losetup --find --show --partscan "$stage/guest.raw")
cleanup() {
    for path in dev proc sys boot/efi ''; do
        mountpoint -q "$root/$path" && umount "$root/$path" || true
    done
    losetup -d "$loop" || true
}
trap cleanup EXIT
test -b "${loop}p1" && test -b "${loop}p2"
mkfs.vfat -F 32 -n HEURISM "${loop}p1"
mkfs.ext4 -q -L heurism-root "${loop}p2"
mount "${loop}p2" "$root"
base=alpine-minirootfs-3.24.2-x86_64.tar.gz
url=https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/x86_64/$base
curl -fL --retry 3 "$url" -o "$stage/$base"
curl -fL --retry 3 "$url.sha256" -o "$stage/$base.sha256"
(cd "$stage" && sha256sum -c "$base.sha256")
tar -xzf "$stage/$base" -C "$root"
mkdir -p "$root"/{dev,proc,sys,boot/efi,etc/companion,var/lib/companion/desktop-stage/evidence}
mount "${loop}p1" "$root/boot/efi"
mount -t proc proc "$root/proc"
mount -t sysfs sysfs "$root/sys"
mount --bind /dev "$root/dev"
printf 'nameserver 1.1.1.1\nnameserver 8.8.8.8\n' > "$root/etc/resolv.conf"
printf '%s\n' https://dl-cdn.alpinelinux.org/alpine/v3.24/main https://dl-cdn.alpinelinux.org/alpine/v3.24/community > "$root/etc/apk/repositories"
chroot "$root" apk add linux-firmware-none
packages=()
while IFS= read -r package || [ -n "$package" ]; do
    case "$package" in ''|'#'*) continue ;; esac
    [[ "$package" =~ ^[a-z0-9][a-z0-9+_.-]*$ ]] || { echo "Invalid package name: $package" >&2; exit 1; }
    packages+=("$package")
done < "$stage/heurism-vm-packages.list"
test "${#packages[@]}" = 46
chroot "$root" apk add "${packages[@]}"
test -L "$root/etc/os-release"
test "$(readlink "$root/etc/os-release")" = ../usr/lib/os-release
grep -Fqx ID=alpine "$root/usr/lib/os-release"
grep -Fqx VERSION_ID=3.24.2 "$root/usr/lib/os-release"
rm "$root/etc/os-release"
install -m 644 "$stage/heurism-os-release" "$root/etc/os-release"
install -d -m 755 "$root/etc/heurism"
install -m 644 "$stage/heurism-upstream-release" "$root/etc/heurism/upstream-release"
install -d -m 755 "$root/usr/share/heurism"
install -m 644 "$stage/heurism-wallpaper.svg" "$root/usr/share/heurism/wallpaper.svg"
printf 'heurism-vm\n' > "$root/etc/hostname"
printf '{"platform":"hyperv-dev"}\n' > "$root/etc/companion/platform.json"
chmod 644 "$root/etc/companion/platform.json"
cat > "$root/etc/network/interfaces" <<'EOF'
auto lo
iface lo inet loopback
auto eth0
iface eth0 inet static
    address 172.28.50.3/24
    gateway 172.28.50.1
EOF
uuid=$(blkid -s UUID -o value "${loop}p2")
esp_uuid=$(blkid -s UUID -o value "${loop}p1")
printf 'UUID=%s / ext4 defaults 0 1\nUUID=%s /boot/efi vfat defaults,umask=0077 0 2\n' "$uuid" "$esp_uuid" > "$root/etc/fstab"
printf 'hv_vmbus\nhv_storvsc\nhv_netvsc\nhyperv_keyboard\nhid_hyperv\nhyperv_fb\n' > "$root/etc/modules"
printf 'blacklist hyperv_drm\n' > "$root/etc/modprobe.d/companion-hyperv.conf"
mkdir -p "$root/etc/mkinitfs/features.d"
printf 'hv_vmbus\nhv_storvsc\nhv_netvsc\nhyperv_keyboard\nhid_hyperv\n' > "$root/etc/mkinitfs/features.d/companion.modules"
printf 'features="base scsi ext4 keyboard companion"\n' > "$root/etc/mkinitfs/mkinitfs.conf"
version=$(basename "$root"/lib/modules/*-lts)
chroot "$root" mkinitfs "$version"
mkdir -p "$root/boot/grub"
cat > "$root/boot/grub/grub.cfg" <<EOF
set timeout_style=hidden
set timeout=1
set default=0
menuentry "Heurism" {
    search --no-floppy --fs-uuid --set=root $uuid
    linux /boot/vmlinuz-lts root=UUID=$uuid rootfstype=ext4 ro quiet video=hyperv_fb:1280x800
    initrd /boot/initramfs-lts
}
EOF
chroot "$root" grub-install --target=x86_64-efi --efi-directory=/boot/efi --boot-directory=/boot --removable --no-nvram
mkdir -p "$root/root/.ssh"
install -m 600 "$stage/client.pub" "$root/root/.ssh/authorized_keys"
chmod 700 "$root/root/.ssh"
cat > "$root/etc/ssh/sshd_config" <<'EOF'
Port 22
PermitRootLogin prohibit-password
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
HostKey /etc/ssh/ssh_host_ed25519_key
Subsystem sftp /usr/lib/ssh/sftp-server
EOF
chroot "$root" ssh-keygen -q -t ed25519 -N '' -f /etc/ssh/ssh_host_ed25519_key
# Local consoles are recoverable without enabling password-based network access.
chroot "$root" passwd -d root
cp "$root/etc/ssh/ssh_host_ed25519_key.pub" "$stage/guest-host.pub"
chroot "$root" adduser -D -h /var/lib/companion/desktop-user -s /sbin/nologin companion-ui
chroot "$root" addgroup companion-ui audio
chroot "$root" addgroup companion-ui video
test "$(chroot "$root" id -u companion-ui)" = 1000
install -d -m 755 "$root/var/lib/heurism/image-source"
tar -xzf "$stage/native.tar.gz" -C "$root/var/lib/heurism/image-source"
chroot "$root" apk add --virtual .heurism-build-deps build-base pkgconf libxft-dev libvterm-dev json-c-dev openssl-dev
chroot "$root" make -C /var/lib/heurism/image-source/native clean all
chroot "$root" sh /var/lib/heurism/image-source/native/install-image-vm.sh
chroot "$root" apk del .heurism-build-deps
if chroot "$root" apk info -e python3-tkinter >/dev/null; then
    echo 'Legacy Tk runtime must not be in the C-first image' >&2
    exit 1
fi
chroot "$root" /opt/heurism/native/current/heurism-release verify
chroot "$root" /opt/heurism/native/current/heurism-release --version
chroot "$root" apk info -v | LC_ALL=C sort > "$root/etc/heurism/packages.installed"
printf 'BASE_MINIROOTFS_SHA256=%s\nNATIVE_SOURCE_SHA256=%s\nPACKAGE_PROFILE_SHA256=%s\n' \
    "$(sha256sum "$stage/$base" | cut -d ' ' -f 1)" \
    "$(sha256sum "$stage/native.tar.gz" | cut -d ' ' -f 1)" \
    "$(sha256sum "$stage/heurism-vm-packages.list" | cut -d ' ' -f 1)" \
    > "$root/etc/heurism/build-provenance"
rm -rf "$root/var/lib/heurism/image-source"
install -d -m 755 "$root/usr/local/sbin"
install -m 755 "$stage/companion-watch" "$root/usr/local/sbin/companion-vm-watch"
install -m 755 "$stage/watch.initd" "$root/etc/init.d/companion-watch"
for service in devfs dmesg udev; do chroot "$root" rc-update add "$service" sysinit; done
for service in modules hwclock swap hostname sysctl bootmisc syslog fsck root localmount udev-trigger udev-settle; do chroot "$root" rc-update add "$service" boot; done
for service in networking sshd dbus companion-watch heurism-control heurism-desktop; do
    chroot "$root" rc-update add "$service" default
done
for service in mount-ro killprocs savecache; do chroot "$root" rc-update add "$service" shutdown; done
chroot "$root" sh -c 'sha256sum /boot/vmlinuz-lts /boot/initramfs-lts /boot/grub/grub.cfg /boot/efi/EFI/BOOT/BOOTX64.EFI /etc/fstab /etc/network/interfaces /etc/ssh/sshd_config /etc/ssh/ssh_host_ed25519_key.pub /etc/init.d/sshd /etc/init.d/companion-watch /etc/init.d/heurism-control /etc/init.d/heurism-desktop /usr/local/sbin/companion-vm-watch /usr/share/heurism/wallpaper.svg /etc/companion/platform.json /etc/os-release /etc/heurism/upstream-release /etc/heurism/packages.installed /etc/heurism/build-provenance > /etc/companion/vm-protected.sha256'
chmod 644 "$root/etc/companion/vm-protected.sha256"
sync
cleanup
trap - EXIT
qemu-img convert -f raw -O vhdx -o subformat=dynamic "$stage/guest.raw" "$stage/companion-dev.vhdx"
sha256sum "$stage/companion-dev.vhdx" > "$stage/companion-dev.vhdx.sha256"
echo HEURISM_VM_IMAGE_READY

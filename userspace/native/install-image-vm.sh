#!/bin/sh
# Seal the C runtime in a fresh Heurism VM image before its first boot.
# Run only inside the image chroot after the UID-1000 account is created.
set -eu
source=/var/lib/heurism/image-source/native
root=/opt/heurism/native
test "$(id -u)" = 0
test -x "$source/heurism-release"
test "$(id -u companion-ui)" = 1000
test ! -e "$root/current"
test ! -L "$root/current"
for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
    command -v "$program" >/dev/null
done
name=heurism-os-image-$(date -u +%Y%m%dT%H%M%SZ)
release=$root/releases/$name
install -d -m 755 "$root/releases" "$release"
for program in heurism-sh heurism-terminal heurism-control heurismctl \
    heurism-desktop heurism-app heurism-session-config heurism-release heurism-slot; do
    install -m 755 "$source/$program" "$release/$program"
done
ln "$release/heurism-app" "$release/heurism-files"
ln "$release/heurism-app" "$release/heurism-editor"
for script in session.sh client.sh user-session.sh xfce-power-panel.sh heurism-look.sh \
    control.initd desktop-heurism.initd; do
    install -m 755 "$source/$script" "$release/$script"
done
for item in openbox.xml heurism-wallpaper.svg heurism-mark.svg \
    heurism-settings.desktop heurism-terminal.desktop \
    heurism-power.desktop xfce4-power-manager.desktop; do
    install -m 644 "$source/$item" "$release/$item"
done
(
    cd "$release"
    sha256sum heurism-sh heurism-terminal heurism-control heurismctl heurism-slot \
        heurism-desktop heurism-app heurism-files heurism-editor \
        heurism-session-config heurism-release session.sh client.sh \
        user-session.sh xfce-power-panel.sh heurism-look.sh \
        heurism-wallpaper.svg heurism-mark.svg control.initd \
        desktop-heurism.initd openbox.xml heurism-settings.desktop \
        heurism-terminal.desktop heurism-power.desktop \
        xfce4-power-manager.desktop > hashes.sha256
)
"$release/heurism-release" verify "$release"
ln -s "$release" "$root/current"
"$release/heurism-release" verify
install -m 755 "$release/control.initd" /etc/init.d/heurism-control
install -m 755 "$release/desktop-heurism.initd" /etc/init.d/heurism-desktop
for program in heurism-sh heurism-terminal heurismctl heurism-release; do
    ln -s "$root/current/$program" "/usr/local/bin/$program"
done
install -d -m 755 /usr/local/sbin
ln -s "$root/current/heurism-slot" /usr/local/sbin/heurism-slot
echo "Heurism C image release sealed: $release"

#!/bin/sh
# Run only after package installation and authenticated staging/hash validation.
set -eu
stage=/var/lib/companion/desktop-stage
stamp=$(date -u +%Y%m%dT%H%M%SZ)
backup=/var/lib/companion/desktop-backup-$stamp
mkdir -p "$backup" "$stage/source" /opt/companion/desktop /usr/local/sbin
apk info > "$backup/packages.txt"
for path in /opt/companion/desktop /etc/init.d/companion-control /etc/init.d/companion-desktop; do
    [ ! -e "$path" ] || cp -a "$path" "$backup/"
done
tar -xzf "$stage/desktop.tar.gz" -C "$stage/source"
id companion-ui >/dev/null 2>&1 || adduser -D -h /var/lib/companion/desktop-user -s /sbin/nologin companion-ui
addgroup companion-ui audio 2>/dev/null || true
addgroup companion-ui video 2>/dev/null || true
release=/opt/companion/releases/$stamp-$$
mkdir -p "$release"
for name in control.py shell.py applications.py system_actions.py platform_config.py home.py input_settings.py session-config.py session.sh client.sh user-session.sh release.py desktop_control_test.py desktop_network_test.py; do
    install -m 755 "$stage/source/$name" "$release/$name"
done
for name in openbox.xml start.html; do
    install -m 644 "$stage/source/$name" "$release/$name"
done
cp "$stage/source/control.initd" "$release/control.initd"
cp "$stage/source/desktop.initd" "$release/desktop.initd"
python3 -m compileall -q "$release"
python3 "$release/desktop_control_test.py"
python3 "$release/desktop_network_test.py"
python3 "$release/release.py" seal "$release"
install -m 755 "$release/release.py" /usr/local/sbin/companion-release
if [ -L /opt/companion/desktop ]; then
    previous=$(readlink -f /opt/companion/desktop)
else
    [ "$(readlink -f /opt/companion/desktop)" = /opt/companion/desktop ] || exit 1
    previous=/opt/companion/releases/legacy-$stamp-$$
    mv /opt/companion/desktop "$previous"
fi
printf '%s\n' "$previous" > /var/lib/companion/desktop-previous-release
ln -s "$release" /opt/companion/desktop.next
mv -T /opt/companion/desktop.next /opt/companion/desktop
install -m 755 "$stage/source/control.initd" /etc/init.d/companion-control
install -m 755 "$stage/source/desktop.initd" /etc/init.d/companion-desktop
python3 /opt/companion/desktop/desktop_control_test.py
sh -n /opt/companion/desktop/session.sh
sh -n /opt/companion/desktop/client.sh
printf 'Installed release %s; services not restarted. Backup: %s\n' "$release" "$backup"

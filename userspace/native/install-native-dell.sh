#!/bin/sh
# Dell release installer. Assembly is inert; activation has a local rollback gate.
set -eu
stage=/var/lib/companion/native-stage
root=/opt/companion/native
candidate_file=/var/lib/companion/native-dell-candidate
protected=/var/lib/companion/presentation-20260927T161814Z/protected.sha256
test "$(id -u)" = 0
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
test ! -e /etc/companion/platform.json
exec 9>/run/companion-native-dell-install.lock
flock -n 9 || { echo 'A Dell native install is already running' >&2; exit 1; }
sha256sum -c "$protected" >/dev/null
rc-service sshd status >/dev/null
rc-service companion-watch status >/dev/null

if [ "${1:-}" = assemble ]; then
    for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
        command -v "$program" >/dev/null
    done
    sh "$stage/native_xfce_bridge.sh" "$stage/companion-desktop" 9>&-
    sh "$stage/native_shell.sh" "$stage/companion-sh" 9>&-
    sh "$stage/native_shell_interactive.sh" "$stage/companion-sh" 9>&-
    sh "$stage/native_control_dell.sh" 9>&-
    if [ ! -e /etc/companion/wifi.conf ]; then sh "$stage/native_wifi_dell.sh" 9>&-; fi
    sh "$stage/native_desktop_dell.sh" 9>&-
    sh "$stage/native_apps_dell.sh" 9>&-
    name=c-os-dell-$(date -u +%Y%m%dT%H%M%SZ)-$$
    release=$root/releases/$name
    test ! -e "$release"
    install -d -m 755 "$release"
    for program in companion-sh companion-terminal companion-control companionctl \
        companion-desktop companion-app companion-session-config companion-release; do
        install -m 755 "$stage/$program" "$release/$program"
    done
    ln "$release/companion-app" "$release/companion-files"
    ln "$release/companion-app" "$release/companion-editor"
    for script in session.sh client.sh user-session.sh xfce-power-panel.sh control.initd desktop.initd; do
        install -m 755 "$stage/$script" "$release/$script"
    done
    install -m 644 "$stage/openbox.xml" "$release/openbox.xml"
    for entry in companion-settings.desktop companion-terminal.desktop companion-power.desktop xfce4-power-manager.desktop; do
        install -m 644 "$stage/$entry" "$release/$entry"
    done
    (
        cd "$release"
        sha256sum companion-sh companion-terminal companion-control companionctl \
            companion-desktop companion-app companion-files companion-editor \
            companion-session-config companion-release session.sh client.sh \
            user-session.sh xfce-power-panel.sh control.initd desktop.initd openbox.xml \
            companion-settings.desktop companion-terminal.desktop companion-power.desktop \
            xfce4-power-manager.desktop >hashes.sha256
    )
    "$release/companion-release" verify "$release"
    printf '%s\n' "$release" >"$candidate_file.new"
    chmod 600 "$candidate_file.new"
    mv -f "$candidate_file.new" "$candidate_file"
    sh "$stage/native_release_dell.sh" 9>&-
    echo "Sealed Dell C candidate: $release"
    exit 0
fi

if [ "${1:-}" != activate ]; then
    echo 'usage: install-native-dell.sh assemble|activate' >&2
    exit 2
fi
candidate=$(cat "$candidate_file")
"$candidate/companion-release" verify "$candidate"
for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
    command -v "$program" >/dev/null
done
previous=$(readlink -f "$root/current")
case "$previous" in "$root"/releases/*) ;; *) echo 'Unexpected current release' >&2; exit 1 ;; esac
test -x /opt/companion/desktop/session.sh
backup=/var/lib/companion/native-dell-init-backup-$(date -u +%Y%m%dT%H%M%SZ)-$$
install -d -m 700 "$backup"
cp -p /etc/init.d/companion-control "$backup/control.initd"
cp -p /etc/init.d/companion-desktop "$backup/desktop.initd"
printf '%s\n' "$previous" >"$backup/previous-release"
printf '%s\n' "$candidate" >"$backup/candidate-release"

stop_desktop() {
    rc-service companion-desktop stop 9>&- >/dev/null 2>&1 || true
    if [ -f /tmp/.X0-lock ]; then
        pid=$(tr -d ' ' </tmp/.X0-lock)
        case "$pid" in ''|*[!0-9]*) return 1 ;; esac
        if [ "$(readlink "/proc/$pid/exe" 2>/dev/null || true)" = /usr/libexec/Xorg ]; then
            kill "$pid" 2>/dev/null || true
        fi
    fi
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        test ! -e /tmp/.X0-lock && return 0
        sleep 1
    done
    return 1
}

restore() {
    trap - EXIT HUP INT TERM
    stop_desktop || true
    rc-service companion-control stop 9>&- >/dev/null 2>&1 || true
    install -m 755 "$backup/control.initd" /etc/init.d/companion-control
    install -m 755 "$backup/desktop.initd" /etc/init.d/companion-desktop
    rm -f "$root/current.rollback"
    ln -s "$previous" "$root/current.rollback"
    mv -fT "$root/current.rollback" "$root/current"
    rc-service companion-control start 9>&-
    rc-service companion-desktop start 9>&-
    echo 'C activation failed; previous desktop and control restored' >&2
}
trap restore EXIT HUP INT TERM
stop_desktop
rc-service companion-control stop 9>&-
install -m 755 "$candidate/control.initd" /etc/init.d/companion-control
install -m 755 "$candidate/desktop.initd" /etc/init.d/companion-desktop
rm -f "$root/current.next"
ln -s "$candidate" "$root/current.next"
mv -fT "$root/current.next" "$root/current"
rc-service companion-control start 9>&-
rc-service companion-desktop start 9>&-
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if "$candidate/companion-release" health >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 1
done
test "$ready" = 1
"$candidate/companion-release" verify "$candidate"
sha256sum -c "$protected" >/dev/null
rc-service companion-control status
rc-service companion-desktop status
rc-service companion-watch status
rc-service sshd status
trap - EXIT HUP INT TERM
echo "Dell C release active: $candidate; rollback files: $backup"

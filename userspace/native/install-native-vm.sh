#!/bin/sh
# Build artifacts enter CompanionDev through the pinned host connection only.
# This guest installer assembles a sealed release, then optionally activates it.
set -eu
stage=/var/lib/companion/native-stage
root=/opt/companion/native
candidate_file=/var/lib/companion/native-candidate-release
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
test -f /etc/companion/platform.json
case "$(cat /etc/companion/platform.json)" in *'"hyperv-dev"'*) ;; *) exit 1 ;; esac

if [ "${1:-}" = assemble ]; then
    for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
        command -v "$program" >/dev/null
    done
    sh "$stage/native_xfce_bridge.sh" "$stage/companion-desktop"
    sh "$stage/native_shell.sh" "$stage/companion-sh"
    sh "$stage/native_shell_interactive.sh" "$stage/companion-sh"
    test_directory=$(mktemp -d "$root/test.XXXXXX")
    install -m 755 "$stage/companion-control" "$stage/companionctl" "$test_directory/"
    chmod 755 "$test_directory"
    sh "$stage/native_control_vm.sh" "$test_directory"
    rm -f "$test_directory/companion-control" "$test_directory/companionctl"
    rmdir "$test_directory"
    name=c-os-$(date -u +%Y%m%dT%H%M%SZ)-$$
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
            xfce4-power-manager.desktop > hashes.sha256
    )
    "$release/companion-release" verify "$release"
    printf '%s\n' "$release" >"$candidate_file.new"
    chmod 600 "$candidate_file.new"
    mv -f "$candidate_file.new" "$candidate_file"
    echo "Native candidate: $release"
    exit 0
fi

if [ "${1:-}" != activate ]; then
    echo 'usage: install-native-vm.sh assemble|activate' >&2
    exit 2
fi
candidate=$(cat "$candidate_file")
"$candidate/companion-release" verify "$candidate"
for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
    command -v "$program" >/dev/null
done
previous=$(readlink -f "$root/current")
backup=/var/lib/companion/native-init-backup-$(date -u +%Y%m%dT%H%M%SZ)-$$
install -d -m 700 "$backup"
cp -p /etc/init.d/companion-control "$backup/control.initd"
cp -p /etc/init.d/companion-desktop "$backup/desktop.initd"
printf '%s\n' "$previous" > /var/lib/companion/native-previous-release

stop_desktop() {
    rc-service companion-desktop stop >/dev/null 2>&1 || true
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
    stop_desktop || true
    rc-service companion-control stop >/dev/null 2>&1 || true
    install -m 755 "$backup/control.initd" /etc/init.d/companion-control
    install -m 755 "$backup/desktop.initd" /etc/init.d/companion-desktop
    ln -s "$previous" "$root/current.rollback"
    mv -fT "$root/current.rollback" "$root/current"
    rc-service companion-control start
    rc-service companion-desktop start
}
trap restore EXIT HUP INT TERM
stop_desktop
rc-service companion-control stop
install -m 755 "$candidate/control.initd" /etc/init.d/companion-control
install -m 755 "$candidate/desktop.initd" /etc/init.d/companion-desktop
ln -s "$candidate" "$root/current.next"
mv -fT "$root/current.next" "$root/current"
rc-service companion-control start
rc-service companion-desktop start
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if "$candidate/companion-release" health >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 1
done
test "$ready" = 1
rc-service companion-control status
rc-service companion-desktop status
rc-service companion-watch status
rc-service sshd status
trap - EXIT HUP INT TERM
echo "Native release active: $candidate; legacy init backup: $backup"

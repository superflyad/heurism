#!/bin/sh
# Build artifacts enter CompanionDev through the pinned host connection only.
# This guest installer assembles a sealed release, then optionally activates it.
set -eu
stage=/var/lib/companion/native-stage
root=/opt/heurism/native
legacy_root=/opt/companion/native
candidate_file=/var/lib/companion/native-candidate-release
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
test -f /etc/companion/platform.json
test ! -L "$stage"
test -d "$stage"
test "$(stat -c %u "$stage")" = 0
test "$(stat -c %a "$stage")" = 700
test -z "$(find "$stage" -maxdepth 1 ! -user root -print -quit)"
test -z "$(find "$stage" -maxdepth 1 -type l -print -quit)"
test -z "$(find "$stage" -maxdepth 1 -type f \( -perm -020 -o -perm -002 \) -print -quit)"
case "$(cat /etc/companion/platform.json)" in *'"hyperv-dev"'*) ;; *) exit 1 ;; esac

if [ "${1:-}" = assemble ]; then
    install -d -m 755 "$root" "$root/releases"
    for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
        command -v "$program" >/dev/null
    done
    sh "$stage/native_xfce_bridge.sh" "$stage/heurism-desktop"
    sh "$stage/native_spaces_vm.sh" "$stage/heurism-desktop"
    sh "$stage/native_shell.sh" "$stage/heurism-sh"
    sh "$stage/native_shell_interactive.sh" "$stage/heurism-sh"
    sh "$stage/native_shell_jobs.sh" "$stage/heurism-sh"
    sh "$stage/native_files_vm.sh" "$stage/heurism-app"
    sh "$stage/native_editor_vm.sh" "$stage/heurism-app"
    test_directory=$(mktemp -d "$root/test.XXXXXX")
    install -m 755 "$stage/heurism-control" "$stage/heurismctl" "$test_directory/"
    chmod 755 "$test_directory"
    sh "$stage/native_control_vm.sh" "$test_directory"
    rm -f "$test_directory/heurism-control" "$test_directory/heurismctl"
    rmdir "$test_directory"
    name=heurism-os-$(date -u +%Y%m%dT%H%M%SZ)-$$
    release=$root/releases/$name
    test ! -e "$release"
    install -d -m 755 "$release"
    for program in heurism-sh heurism-terminal heurism-control heurismctl \
        heurism-desktop heurism-app heurism-session-config heurism-release; do
        install -m 755 "$stage/$program" "$release/$program"
    done
    ln "$release/heurism-app" "$release/heurism-files"
    ln "$release/heurism-app" "$release/heurism-editor"
    for script in session.sh client.sh user-session.sh xfce-power-panel.sh heurism-look.sh control.initd desktop.initd desktop-heurism.initd; do
        install -m 755 "$stage/$script" "$release/$script"
    done
    install -m 644 "$stage/openbox.xml" "$release/openbox.xml"
    install -m 644 "$stage/heurism-wallpaper.svg" "$stage/heurism-mark.svg" \
        "$stage/heurism-xfwm4.tar.gz" "$release/"
    for entry in heurism-settings.desktop heurism-terminal.desktop heurism-power.desktop xfce4-power-manager.desktop; do
        install -m 644 "$stage/$entry" "$release/$entry"
    done
    (
        cd "$release"
        sha256sum heurism-sh heurism-terminal heurism-control heurismctl \
            heurism-desktop heurism-app heurism-files heurism-editor \
            heurism-session-config heurism-release session.sh client.sh \
            user-session.sh xfce-power-panel.sh heurism-look.sh heurism-wallpaper.svg heurism-mark.svg heurism-xfwm4.tar.gz \
            control.initd desktop.initd desktop-heurism.initd openbox.xml \
            heurism-settings.desktop heurism-terminal.desktop heurism-power.desktop \
            xfce4-power-manager.desktop > hashes.sha256
    )
    "$release/heurism-release" verify "$release"
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
"$candidate/heurism-release" verify "$candidate"
for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
    command -v "$program" >/dev/null
done
if [ -L "$root/current" ]; then
    previous=$(readlink -f "$root/current")
else
    previous=$(readlink -f "$legacy_root/current")
fi
case "$previous" in "$root"/releases/*|"$legacy_root"/releases/*) ;; *) exit 1 ;; esac
control=companion-control
desktop=companion-desktop
desktop_script=desktop.initd
if [ -L /etc/runlevels/default/heurism-control ] &&
   [ -L /etc/runlevels/default/heurism-desktop ]; then
    control=heurism-control
    desktop=heurism-desktop
    desktop_script=desktop-heurism.initd
fi
backup=/var/lib/companion/native-init-backup-$(date -u +%Y%m%dT%H%M%SZ)-$$
created_links=
install -d -m 700 "$backup"
cp -p "/etc/init.d/$control" "$backup/control.initd"
cp -p "/etc/init.d/$desktop" "$backup/desktop.initd"
case "$previous" in "$root"/releases/*)
    printf '%s\n' "$previous" > /var/lib/companion/native-previous-release ;;
esac

stop_desktop() {
    rc-service "$desktop" stop >/dev/null 2>&1 || true
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
    for program in $created_links; do rm -f "/usr/local/bin/$program"; done
    stop_desktop || true
    rc-service "$control" stop >/dev/null 2>&1 || true
    install -m 755 "$backup/control.initd" "/etc/init.d/$control"
    install -m 755 "$backup/desktop.initd" "/etc/init.d/$desktop"
    case "$previous" in
        "$root"/releases/*)
            rm -f "$root/current.rollback"
            ln -s "$previous" "$root/current.rollback"
            mv -fT "$root/current.rollback" "$root/current" ;;
        "$legacy_root"/releases/*) rm -f "$root/current" ;;
    esac
    rc-service "$control" start
    rc-service "$desktop" start
}
trap restore EXIT HUP INT TERM
stop_desktop
rc-service "$control" stop
install -m 755 "$candidate/control.initd" "/etc/init.d/$control"
install -m 755 "$candidate/$desktop_script" "/etc/init.d/$desktop"
rm -f "$root/current.next"
ln -s "$candidate" "$root/current.next"
mv -fT "$root/current.next" "$root/current"
rc-service "$control" start
rc-service "$desktop" start
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if "$candidate/heurism-release" health >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 1
done
test "$ready" = 1
rc-service "$control" status
rc-service "$desktop" status
rc-service companion-watch status
rc-service sshd status
for program in heurism-sh heurism-terminal heurismctl heurism-release; do
    link=/usr/local/bin/$program
    target=$root/current/$program
    if [ -L "$link" ]; then
        test "$(readlink "$link")" = "$target"
    else
        test ! -e "$link"
        ln -s "$target" "$link"
        created_links="$created_links $program"
    fi
done
trap - EXIT HUP INT TERM
echo "Native release active: $candidate; legacy init backup: $backup"

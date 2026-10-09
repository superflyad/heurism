#!/bin/sh
# Isolated Xfce session and application smoke test; never touches DISPLAY=:0.
set -eu
test "$(id -u)" = 0
test -x /usr/bin/xfce4-session
test -x /usr/bin/thunar
test -x /usr/bin/mousepad
settings=${1:-/opt/companion/native/current/companion-desktop}
test -x "$settings"
stage=$(mktemp -d /tmp/companion-xfce-test.XXXXXX)
cleanup() {
    kill "${session:-}" "${xserver:-}" 2>/dev/null || true
    wait "${session:-}" "${xserver:-}" 2>/dev/null || true
    rm -rf "$stage"
}
trap cleanup EXIT INT TERM
mkdir -p "$stage/home/Documents" "$stage/run"
chown -R companion-ui:companion-ui "$stage"
chmod 700 "$stage" "$stage/home" "$stage/run"
cp "$settings" "$stage/companion-desktop"
chmod 755 "$stage/companion-desktop"
settings="$stage/companion-desktop"
Xvfb :2 -screen 0 1280x800x24 -nolisten tcp >"$stage/x.log" 2>&1 &
xserver=$!
sleep 1
test -d /proc/"$xserver"
su -s /bin/sh -c "HOME='$stage/home' XDG_RUNTIME_DIR='$stage/run' DISPLAY=:2 dbus-run-session -- xfce4-session >'$stage/session.log' 2>&1" companion-ui &
session=$!
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if pgrep -u companion-ui -x xfwm4 >/dev/null &&
       pgrep -u companion-ui -x xfce4-panel >/dev/null &&
       pgrep -u companion-ui -x xfdesktop >/dev/null &&
       DISPLAY=:2 xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; then
        ready=1
        break
    fi
    sleep 1
done
if [ "$ready" != 1 ]; then cat "$stage/session.log"; exit 1; fi
su -s /bin/sh -c "HOME='$stage/home' XDG_RUNTIME_DIR='$stage/run' DISPLAY=:2 thunar '$stage/home/Documents' >'$stage/files.log' 2>&1 &" companion-ui
su -s /bin/sh -c "HOME='$stage/home' XDG_RUNTIME_DIR='$stage/run' DISPLAY=:2 mousepad >'$stage/editor.log' 2>&1 &" companion-ui
su -s /bin/sh -c "HOME='$stage/home' XDG_RUNTIME_DIR='$stage/run' DISPLAY=:2 '$settings' --settings >'$stage/settings.log' 2>&1 &" companion-ui
windows=0
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    if DISPLAY=:2 xdotool search --onlyvisible --class Thunar >/dev/null &&
       DISPLAY=:2 xdotool search --onlyvisible --class Mousepad >/dev/null &&
       DISPLAY=:2 xdotool search --onlyvisible --name 'Companion Settings' >/dev/null; then
        windows=1
        break
    fi
    sleep 1
done
if [ "$windows" != 1 ]; then
    cat "$stage/files.log" "$stage/editor.log" "$stage/settings.log"
    exit 1
fi
if DISPLAY=:2 xdotool search --onlyvisible --name 'Companion dock' >/dev/null 2>&1; then
    echo 'Settings mode incorrectly mapped the native dock' >&2
    exit 1
fi
su -s /bin/sh -c "HOME='$stage/home' XDG_RUNTIME_DIR='$stage/run' DISPLAY=:2 '$settings' --power >'$stage/power.log' 2>&1 &" companion-ui
power=0
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    if DISPLAY=:2 xdotool search --onlyvisible --name 'Companion Power' >/dev/null; then
        power=1
        break
    fi
    sleep 1
done
if [ "$power" != 1 ]; then cat "$stage/power.log"; exit 1; fi
DISPLAY=:2 xdotool search --onlyvisible --name 'Companion Power' |
    head -n 1 | xargs -r -I{} env DISPLAY=:2 xdotool windowactivate {} key Escape
sleep 1
if DISPLAY=:2 xdotool search --onlyvisible --name 'Companion Power' >/dev/null 2>&1; then
    echo 'Companion Power did not close on Escape' >&2
    exit 1
fi
echo 'isolated Xfce windows, apps, C Settings and C Power passed'

#!/bin/sh
# Select the established Xfce shell while Heurism owns the C controls/apps.
set -eu
state="$HOME/.local/state/heurism"
release=/opt/heurism/native/current
mkdir -p "$state"
pulseaudio --start --exit-idle-time=-1 >>"$state/audio.log" 2>&1 || true
if [ "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.' ] &&
   [ "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1' ]; then
    applied=0
    for attempt in 1 2 3 4 5; do
        if "$release/heurismctl" input-apply-saved >>"$state/input-native.log" 2>&1; then
            applied=1
            break
        fi
        sleep 1
    done
    test "$applied" = 1
fi

mode=xfce
if [ -r /etc/companion/native-session-mode ]; then
    mode=$(cat /etc/companion/native-session-mode)
fi
if [ "$mode" = native ]; then
    openbox --config-file "$release/openbox.xml" >"$state/openbox-native.log" 2>&1 &
    wm=$!
    ready=0
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        if xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; then
            ready=1
            break
        fi
        kill -0 "$wm" 2>/dev/null || break
        sleep 1
    done
    if [ "$ready" = 0 ]; then
        kill "$wm" 2>/dev/null || true
        wait "$wm" 2>/dev/null || true
        exit 1
    fi
    if "$release/heurism-desktop"; then result=0; else result=$?; fi
    kill "$wm" 2>/dev/null || true
    wait "$wm" 2>/dev/null || true
    exit "$result"
fi
test "$mode" = xfce || { echo "Unsupported Heurism session: $mode" >&2; exit 1; }
for program in xfce4-session xfwm4 xfce4-panel xfdesktop thunar mousepad; do
    command -v "$program" >/dev/null
done
mkdir -p "$HOME/.local/share/applications" "$HOME/.config/autostart"
cp "$release/heurism-settings.desktop" "$release/heurism-terminal.desktop" \
    "$release/heurism-power.desktop" \
    "$HOME/.local/share/applications/"
rm -f "$HOME/.local/share/applications/companion-settings.desktop" \
    "$HOME/.local/share/applications/companion-terminal.desktop" \
    "$HOME/.local/share/applications/companion-power.desktop"
cp "$release/heurism-power.desktop" \
    "$HOME/.local/share/applications/xfce4-session-logout.desktop"
cp "$release/xfce4-power-manager.desktop" "$HOME/.config/autostart/"
export XDG_CURRENT_DESKTOP=XFCE DESKTOP_SESSION=xfce XDG_SESSION_DESKTOP=xfce
export XDG_SESSION_TYPE=x11
xfce4-session >"$state/xfce-session.log" 2>&1 &
session=$!
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if kill -0 "$session" 2>/dev/null &&
       pgrep -u "$(id -u)" -x xfwm4 >/dev/null &&
       pgrep -u "$(id -u)" -x xfce4-panel >/dev/null &&
       pgrep -u "$(id -u)" -x xfdesktop >/dev/null &&
       xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; then
        ready=1
        break
    fi
    sleep 1
done
if [ "$ready" = 0 ]; then
    kill "$session" 2>/dev/null || true
    wait "$session" 2>/dev/null || true
    echo 'Xfce session did not become healthy' >&2
    exit 1
fi
sh "$release/xfce-power-panel.sh" >>"$state/xfce-panel.log" 2>&1
umask 077
resolved=$(readlink -f "$release")
boot=$(cat /proc/sys/kernel/random/boot_id)
health="$state/session-health-native.json"
temporary="$health.tmp.$$"
printf '{"pid":%s,"uid":%s,"release":"%s","boot_id":"%s","session":"xfce","page":"workspace"}\n' \
    "$session" "$(id -u)" "$resolved" "$boot" >"$temporary"
mv -f "$temporary" "$health"
wait "$session"

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

native_session() {
    if [ "${1:-}" = replace ]; then
        openbox --replace --config-file "$release/openbox.xml" >"$state/openbox-native.log" 2>&1 &
    else
        openbox --config-file "$release/openbox.xml" >"$state/openbox-native.log" 2>&1 &
    fi
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
}

heurism_session() {
    # Xfwm supplies established window management; the C workspace owns the
    # visible desktop and dock. A locked machine must verify its PAM locker
    # before the workspace can publish health.
    command -v xfwm4 >/dev/null
    command -v xfsettingsd >/dev/null
    command -v xfce4-screensaver >/dev/null
    command -v xfce4-screensaver-command >/dev/null
    command -v xfconf-query >/dev/null
    command -v tar >/dev/null
    theme="$HOME/.themes/Heurism/xfwm4"
    mkdir -p "$theme"
    tar -xzf "$release/heurism-xfwm4.tar.gz" -C "$theme"
    xfconf-query -c xfwm4 -p /general/theme -n -t string -s Heurism
    export XDG_CURRENT_DESKTOP=XFCE DESKTOP_SESSION=heurism XDG_SESSION_DESKTOP=heurism
    export XDG_SESSION_TYPE=x11
    xfsettingsd >"$state/heurism-settings-daemon.log" 2>&1 &
    settings_daemon=$!
    xfwm4 >"$state/heurism-window-manager.log" 2>&1 &
    wm=$!
    ready=0
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        if kill -0 "$wm" 2>/dev/null &&
           xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; then
            ready=1
            break
        fi
        sleep 1
    done
    test "$ready" = 1 || return 1
    xfce4-screensaver >"$state/heurism-locker.log" 2>&1 &
    if [ -e /etc/heurism/desktop-lock ]; then
        locked=0
        for attempt in 1 2 3 4 5 6 7 8 9 10; do
            xfce4-screensaver-command --lock >/dev/null 2>&1 || true
            if LC_ALL=C xfce4-screensaver-command --query 2>/dev/null |
               grep -Fxq 'The screensaver is active'; then
                locked=1
                break
            fi
            sleep 1
        done
        test "$locked" = 1 || return 1
    fi
    "$release/heurism-desktop" >"$state/heurism-workspace.log" 2>&1 &
    workspace=$!
    while kill -0 "$workspace" 2>/dev/null; do
        if ! kill -0 "$wm" 2>/dev/null ||
           ! kill -0 "$settings_daemon" 2>/dev/null ||
           { [ -e /etc/heurism/desktop-lock ] &&
             ! pgrep -u "$(id -u)" -f '^xfce4-screensaver($| )' >/dev/null; }; then
            kill "$workspace" 2>/dev/null || true
            wait "$workspace" 2>/dev/null || true
            return 1
        fi
        sleep 1
    done
    wait "$workspace"
}

mode=xfce
if [ -r /etc/companion/native-session-mode ]; then
    mode=$(cat /etc/companion/native-session-mode)
fi
if [ -e /etc/heurism/desktop-lock ] && [ "$mode" != xfce ] &&
   [ "$mode" != heurism ]; then
    echo 'Locked desktop requires the Xfce session' >&2
    exit 1
fi
if [ "$mode" = native ]; then
    native_session
fi
if [ "$mode" = heurism ]; then
    heurism_session
    exit $?
fi
if [ "$mode" != xfce ]; then
    echo "Unsupported Heurism session: $mode; starting C workspace" >&2
    native_session
fi
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
    if [ -e /etc/heurism/desktop-lock ]; then
        echo 'Xfce failed before lock; authenticated desktop unavailable' >&2
        exit 1
    fi
    echo 'Xfce session did not become healthy; starting C workspace' >&2
    native_session replace
fi
sh "$release/xfce-power-panel.sh" >>"$state/xfce-panel.log" 2>&1
sh "$release/heurism-look.sh" >>"$state/xfce-look.log" 2>&1 ||
    echo 'Heurism look could not be applied; Xfce remains usable' >&2
if [ -e /etc/heurism/desktop-lock ]; then
    command -v xfce4-screensaver-command >/dev/null
    locked=0
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        xfce4-screensaver-command --lock >/dev/null 2>&1 || true
        if LC_ALL=C xfce4-screensaver-command --query 2>/dev/null |
           grep -Fxq 'The screensaver is active'; then
            locked=1
            break
        fi
        sleep 1
    done
    if [ "$locked" != 1 ]; then
        echo 'Desktop lock did not activate' >&2
        kill "$session" 2>/dev/null || true
        wait "$session" 2>/dev/null || true
        exit 1
    fi
fi

umask 077
resolved=$(readlink -f "$release")
boot=$(cat /proc/sys/kernel/random/boot_id)
health="$state/session-health-native.json"
temporary="$health.tmp.$$"
printf '{"pid":%s,"uid":%s,"release":"%s","boot_id":"%s","session":"xfce","page":"workspace"}\n' \
    "$session" "$(id -u)" "$resolved" "$boot" >"$temporary"
mv -f "$temporary" "$health"
if [ -e /etc/heurism/desktop-lock ]; then
    while kill -0 "$session" 2>/dev/null; do
        if ! pgrep -u "$(id -u)" -f '^xfce4-screensaver($| )' >/dev/null; then
            echo 'Desktop lock process exited; ending graphical session' >&2
            kill "$session" 2>/dev/null || true
            wait "$session" 2>/dev/null || true
            exit 1
        fi
        sleep 1
    done
fi
wait "$session"

#!/bin/sh
# Exercise the C launcher against a sealed release on an isolated VM display.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
release=$(readlink -f "${1:-/opt/heurism/native/current}")
case "$release" in /opt/heurism/native/releases/*) ;; *) exit 1 ;; esac
"$release/heurism-release" verify "$release" >/dev/null
saved_theme=$("$release/heurismctl" status |
    sed -n 's/.*"theme":"\([^"]*\)".*/\1/p')
case "$saved_theme" in light|night) ;; *) exit 1 ;; esac
for program in Xvfb xfwm4 xfconf-query xdotool xprop xwd dbus-run-session \
    xfce4-screensaver-command; do
    command -v "$program" >/dev/null
done
test ! -e /tmp/.X91-lock
work=$(mktemp -d /tmp/heurism-launcher-test.XXXXXX)
chown companion-ui:companion-ui "$work"
chmod 700 "$work"
install -d -o companion-ui -g companion-ui -m 700 "$work/run"
install -d -o companion-ui -g companion-ui -m 755 \
    "$work/.themes" "$work/.themes/Heurism" "$work/.themes/Heurism/xfwm4"
su -s /bin/sh -c "tar -xzf '$release/heurism-xfwm4.tar.gz' -C '$work/.themes/Heurism/xfwm4'" companion-ui
Xvfb :91 -screen 0 1280x800x24 -nolisten tcp -ac >"$work/xvfb.log" 2>&1 &
xvfb=$!
wm=0
desktop=0
cleanup() {
    "$release/heurismctl" theme "\"$saved_theme\"" >/dev/null 2>&1 || true
    test "$desktop" = 0 || kill "$desktop" 2>/dev/null || true
    test "$wm" = 0 || kill "$wm" 2>/dev/null || true
    for environment in /proc/[0-9]*/environ; do
        test -r "$environment" || continue
        pid=${environment#/proc/}
        pid=${pid%/environ}
        test "$(stat -c %u "/proc/$pid" 2>/dev/null || true)" = "$(id -u companion-ui)" || continue
        if cat "$environment" 2>/dev/null | tr '\000' '\n' |
           grep -Fxq "XDG_RUNTIME_DIR=$work/run"; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    kill "$xvfb" 2>/dev/null || true
    wait "$xvfb" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM
sleep 2
su -s /bin/sh -c "HOME=$work DISPLAY=:91 XAUTHORITY=/dev/null XDG_RUNTIME_DIR=$work/run dbus-run-session sh -c 'xfconf-query -c xfwm4 -p /general/theme -n -t string -s Heurism; exec xfwm4'" companion-ui >"$work/xfwm.log" 2>&1 &
wm=$!
sleep 3
su -s /bin/sh -c "HOME=$work DISPLAY=:91 XAUTHORITY=/dev/null XDG_RUNTIME_DIR=$work/run exec '$release/heurism-desktop'" companion-ui >"$work/desktop.log" 2>&1 &
desktop=$!
sleep 3
export DISPLAY=:91 XAUTHORITY=/dev/null
xwd -root -silent -out "$work/workspace.xwd"
xdotool mousemove 180 748 click 1
sleep 2
launcher=$(xdotool search --name '^Heurism Launcher$' | tail -n 1)
test -n "$launcher"
xprop -id "$launcher" _NET_WM_STATE | grep -q '_NET_WM_STATE_SKIP_TASKBAR'
xdotool mousemove 180 748 click 1
sleep 1
test "$(xdotool search --name '^Heurism Launcher$' | wc -l)" = 1
xdotool windowactivate --sync "$launcher"
xdotool type --clearmodifiers 'term'
sleep 1
xwd -root -silent -out "$work/launcher.xwd"
xdotool key Return
sleep 2
terminal=$(xdotool search --name 'Heurism C Terminal' | tail -n 1)
test -n "$terminal"
if xdotool search --name '^Heurism Launcher$' >/dev/null 2>&1; then exit 1; fi
xdotool windowactivate --sync "$terminal"
xdotool key --clearmodifiers super+space
sleep 1
launcher=$(xdotool search --name '^Heurism Launcher$' | tail -n 1)
test -n "$launcher"
xdotool windowactivate --sync "$launcher"
xdotool key Escape
sleep 1
if xdotool search --name '^Heurism Launcher$' >/dev/null 2>&1; then exit 1; fi
xdotool mousemove 286 748 click 1
sleep 2
files=$(xdotool search --name '^Heurism Files$' | tail -n 1)
test -n "$files"
xdotool windowactivate --sync "$files"
xdotool key --clearmodifiers super+space
sleep 1
launcher=$(xdotool search --name '^Heurism Launcher$' | tail -n 1)
test -n "$launcher"
xdotool windowactivate --sync "$launcher"
xdotool type --clearmodifiers 'heurism c'
sleep 1
xwd -root -silent -out "$work/window-switch.xwd"
xdotool key Return
sleep 2
test "$(xdotool getactivewindow)" = "$terminal"
if xdotool search --name '^Heurism Launcher$' >/dev/null 2>&1; then exit 1; fi
xdotool mousemove 1000 748 click 1
sleep 1
quick=$(xdotool search --name '^Heurism Quick Controls$' | tail -n 1)
test -n "$quick"
xprop -id "$quick" _NET_WM_STATE | grep -q '_NET_WM_STATE_SKIP_TASKBAR'
xdotool mousemove 1000 748 click 1
sleep 1
test "$(xdotool search --name '^Heurism Quick Controls$' | wc -l)" = 1
xwd -root -silent -out "$work/quick-controls.xwd"
if [ "$saved_theme" = night ]; then opposite=light; else opposite=night; fi
xdotool mousemove --window "$quick" 160 285 click 1
sleep 2
current_theme=$("$release/heurismctl" status |
    sed -n 's/.*"theme":"\([^"]*\)".*/\1/p')
test "$current_theme" = "$opposite"
xwd -root -silent -out "$work/quick-controls-toggled.xwd"
"$release/heurismctl" theme "\"$saved_theme\"" >/dev/null
sleep 1
xdotool mousemove --window "$quick" 80 395 click 1
sleep 2
xdotool search --name '^Heurism Settings$' >/dev/null
if xdotool search --name '^Heurism Quick Controls$' >/dev/null 2>&1; then exit 1; fi
xdotool mousemove 1000 748 click 1
sleep 1
quick=$(xdotool search --name '^Heurism Quick Controls$' | tail -n 1)
xdotool mousemove --window "$quick" 365 395 click 1
sleep 2
xdotool search --name '^Heurism Network$' >/dev/null
xdotool mousemove 1000 748 click 1
sleep 1
quick=$(xdotool search --name '^Heurism Quick Controls$' | tail -n 1)
xdotool mousemove --window "$quick" 365 460 click 1
sleep 2
xdotool search --name '^Heurism Power$' >/dev/null
xdotool mousemove 1000 748 click 1
sleep 1
quick=$(xdotool search --name '^Heurism Quick Controls$' | tail -n 1)
xdotool windowactivate --sync "$quick"
xdotool key Escape
sleep 1
if xdotool search --name '^Heurism Quick Controls$' >/dev/null 2>&1; then exit 1; fi
for action in settings power; do
    xdotool mousemove 180 748 click 1
    sleep 1
    launcher=$(xdotool search --name '^Heurism Launcher$' | tail -n 1)
    xdotool windowactivate --sync "$launcher"
    xdotool type --clearmodifiers "$action"
    xdotool key Return
    sleep 2
    if [ "$action" = settings ]; then title='Heurism Settings'; else title='Heurism Power'; fi
    xdotool search --name "^$title$" >/dev/null
done
xdotool mousemove 180 748 click 1
sleep 1
launcher=$(xdotool search --name '^Heurism Launcher$' | tail -n 1)
xdotool windowactivate --sync "$launcher"
xdotool key Escape
sleep 1
if xdotool search --name '^Heurism Launcher$' >/dev/null 2>&1; then exit 1; fi
echo "C launcher, window switching and quick controls passed; captures: $work/workspace.xwd $work/launcher.xwd $work/window-switch.xwd $work/quick-controls.xwd"

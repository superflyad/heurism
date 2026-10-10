#!/bin/sh
# Exercise the candidate C Spaces view and cross-workspace window activation.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
candidate=${1:-/var/lib/companion/native-stage/heurism-desktop}
test -x "$candidate" && test ! -L "$candidate"
for program in Xvfb xfwm4 xfconf-query xdotool xprop xwd dbus-run-session tar; do
    command -v "$program" >/dev/null
done
test ! -e /tmp/.X95-lock
release=$(readlink -f /opt/heurism/native/current)
work=$(mktemp -d /tmp/heurism-spaces-test.XXXXXX)
chown companion-ui:companion-ui "$work"
chmod 700 "$work"
install -d -o companion-ui -g companion-ui -m 700 "$work/run"
install -d -o companion-ui -g companion-ui -m 755 "$work/.themes/Heurism/xfwm4"
tar -xzf "$release/heurism-xfwm4.tar.gz" -C "$work/.themes/Heurism/xfwm4"
chown -R companion-ui:companion-ui "$work/.themes"
install -m 755 "$candidate" "$work/heurism-desktop"
Xvfb :95 -screen 0 1280x800x24 -nolisten tcp -ac >"$work/xvfb.log" 2>&1 &
xvfb=$!
wm=0 desktop=0 terminal=0
cleanup() {
    test "$desktop" = 0 || kill "$desktop" 2>/dev/null || true
    test "$terminal" = 0 || kill "$terminal" 2>/dev/null || true
    test "$wm" = 0 || kill "$wm" 2>/dev/null || true
    for environment in /proc/[0-9]*/environ; do
        test -r "$environment" || continue
        pid=${environment#/proc/}; pid=${pid%/environ}
        test "$(stat -c %u "/proc/$pid" 2>/dev/null || true)" = \
            "$(id -u companion-ui)" || continue
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
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:95 XAUTHORITY=/dev/null dbus-run-session sh -c 'xfconf-query -c xfwm4 -p /general/theme -n -t string -s Heurism; exec xfwm4'" companion-ui >"$work/wm.log" 2>&1 &
wm=$!
sleep 3
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:95 XAUTHORITY=/dev/null exec $work/heurism-desktop" companion-ui >"$work/desktop.log" 2>&1 &
desktop=$!
sleep 3
export DISPLAY=:95 XAUTHORITY=/dev/null
xdotool search --name '^Heurism panel$' >/dev/null
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:95 XAUTHORITY=/dev/null exec $release/heurism-terminal" companion-ui >"$work/terminal.log" 2>&1 &
terminal=$!
sleep 2
window=$(xdotool search --name 'Heurism C Terminal' | tail -n 1)
test -n "$window"
xdotool windowactivate --sync "$window"
xdotool key --clearmodifiers super+shift+2
sleep 1
xprop -id "$window" _NET_WM_DESKTOP | grep -q ' = 1$'
xdotool key --clearmodifiers super+1
sleep 1
xdotool mousemove 190 24 click 1
sleep 2
spaces=$(xdotool search --name '^Heurism Spaces$' | tail -n 1)
test -n "$spaces"
xprop -id "$spaces" _NET_WM_STATE | grep -q '_NET_WM_STATE_SKIP_TASKBAR'
xwd -root -silent -out "$work/spaces-with-window.xwd"
xdotool mousemove --window "$spaces" 865 230 click 1
sleep 1
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 1$'
test "$(xdotool getactivewindow)" = "$window"
if xdotool search --name '^Heurism Spaces$' >/dev/null 2>&1; then exit 1; fi
xdotool key --clearmodifiers super+o
sleep 1
spaces=$(xdotool search --name '^Heurism Spaces$' | tail -n 1)
test -n "$spaces"
xdotool windowactivate --sync "$spaces"
xdotool key 1
sleep 1
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 0$'
if xdotool search --name '^Heurism Spaces$' >/dev/null 2>&1; then exit 1; fi
echo "C Spaces view, occupied workspace, cross-space window activation and keyboard navigation passed; capture: $work/spaces-with-window.xwd"

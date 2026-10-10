#!/bin/sh
# Check a candidate C workspace at the Dell display size without changing :0.
set -eu
test "$(id -u)" = 0
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
test ! -e /tmp/.X93-lock
candidate=${1:-/var/lib/companion/native-stage/heurism-desktop}
test -f "$candidate" && test -x "$candidate" && test ! -L "$candidate"
test "$(stat -c %u "$candidate")" = 0
test "$(stat -c %a "$candidate" | cut -c 1)" != 0
"$candidate" --version
for program in Xvfb xfwm4 xdotool xprop xwd dbus-run-session xfconf-query tar xauth; do
    command -v "$program" >/dev/null
done
release=$(readlink -f /opt/heurism/native/current)
"$release/heurism-release" verify "$release" >/dev/null
theme_archive=${2:-$release/heurism-xfwm4.tar.gz}
test -f "$theme_archive"
work=$(mktemp -d /tmp/heurism-workspace-dell.XXXXXX)
xvfb=0 wm=0 desktop=0
cleanup() {
    test "$desktop" = 0 || { kill "$desktop" 2>/dev/null || true; wait "$desktop" 2>/dev/null || true; }
    test "$wm" = 0 || { kill "$wm" 2>/dev/null || true; wait "$wm" 2>/dev/null || true; }
    for environment in /proc/[0-9]*/environ; do
        test -r "$environment" || continue
        pid=${environment#/proc/}; pid=${pid%/environ}
        test "$(stat -c %u "/proc/$pid" 2>/dev/null || true)" = "$(id -u companion-ui)" || continue
        if cat "$environment" 2>/dev/null | tr '\000' '\n' |
           grep -Fxq "XDG_RUNTIME_DIR=$work/run"; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    test "$xvfb" = 0 || { kill "$xvfb" 2>/dev/null || true; wait "$xvfb" 2>/dev/null || true; }
    case "$work" in /tmp/heurism-workspace-dell.*) rm -rf "$work" ;; esac
}
trap cleanup EXIT HUP INT TERM
chown companion-ui:companion-ui "$work"
chmod 700 "$work"
install -d -o companion-ui -g companion-ui -m 700 "$work/run" "$work/config"
install -d -o companion-ui -g companion-ui -m 755 "$work/Documents"
printf 'Dell search fixture\n' >"$work/Documents/orbitnote.txt"
chown companion-ui:companion-ui "$work/Documents/orbitnote.txt"
install -d -o companion-ui -g companion-ui -m 755 "$work/.themes/Heurism/xfwm4"
cookie=$(od -An -N16 -tx1 /dev/urandom | tr -d '[:space:]')
: >"$work/Xauthority"
xauth -f "$work/Xauthority" add :93 . "$cookie" >/dev/null
chown companion-ui:companion-ui "$work/Xauthority"
chmod 600 "$work/Xauthority"
tar -xzf "$theme_archive" -C "$work/.themes/Heurism/xfwm4"
chown -R companion-ui:companion-ui "$work/.themes"
install -m 755 "$candidate" "$work/heurism-desktop"
ln -s "$release/heurism-terminal" "$work/heurism-terminal"
ln -s "$release/heurism-files" "$work/heurism-files"
ln -s "$release/heurism-editor" "$work/heurism-editor"
Xvfb :93 -screen 0 1920x1080x24 -nolisten tcp -auth "$work/Xauthority" >"$work/xvfb.log" 2>&1 &
xvfb=$!
sleep 2
su -s /bin/sh -c "HOME=$work XDG_CONFIG_HOME=$work/config DISPLAY=:93 XAUTHORITY=$work/Xauthority XDG_RUNTIME_DIR=$work/run dbus-run-session sh -c 'xfconf-query -c xfwm4 -p /general/theme -n -t string -s Heurism; exec xfwm4'" companion-ui >"$work/xfwm.log" 2>&1 &
wm=$!
sleep 3
su -s /bin/sh -c "HOME=$work XDG_CONFIG_HOME=$work/config DISPLAY=:93 XAUTHORITY=$work/Xauthority XDG_RUNTIME_DIR=$work/run exec '$work/heurism-desktop'" companion-ui >"$work/desktop.log" 2>&1 &
desktop=$!
sleep 3
export DISPLAY=:93 XAUTHORITY="$work/Xauthority"
xdotool search --name '^Heurism dock$' >/dev/null
xdotool search --name '^Heurism panel$' >/dev/null
xdotool getdisplaygeometry | grep -Fxq '1920 1080'
xprop -root _NET_NUMBER_OF_DESKTOPS | grep -q ' = 4$'
xdotool mousemove 305 24 click 1
sleep 1
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 1$'
xdotool mousemove 262 24 click 1
sleep 1
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 0$'
mkdir -p /var/lib/companion/native-stage/evidence
xwd -root -silent -out /var/lib/companion/native-stage/evidence/heurism-dell-workspace.xwd
xdotool mousemove 1270 1028 click 1
sleep 2
quick=$(xdotool search --name '^Heurism Quick Controls$' | tail -n 1)
test -n "$quick"
xprop -id "$quick" _NET_WM_STATE | grep -q '_NET_WM_STATE_SKIP_TASKBAR'
xwd -root -silent -out /var/lib/companion/native-stage/evidence/heurism-dell-quick.xwd
xdotool windowactivate --sync "$quick"
xdotool key Escape
sleep 1
if xdotool search --name '^Heurism Quick Controls$' >/dev/null 2>&1; then exit 1; fi
xdotool mousemove 640 1028 click 1
sleep 1
launcher=$(xdotool search --name '^Heurism Launcher$' | tail -n 1)
test -n "$launcher"
xdotool windowactivate --sync "$launcher"
xdotool type --clearmodifiers 'terminal'
xdotool key Return
sleep 2
terminal=$(xdotool search --name 'Heurism C Terminal' | tail -n 1)
test -n "$terminal"
xdotool windowactivate --sync "$terminal"
xdotool key --clearmodifiers super+shift+2
sleep 1
xprop -id "$terminal" _NET_WM_DESKTOP | grep -q ' = 1$'
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 1$'
xdotool key --clearmodifiers super+1
sleep 1
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 0$'
xdotool mousemove 190 24 click 1
sleep 1
spaces=$(xdotool search --name '^Heurism Spaces$' | tail -n 1)
test -n "$spaces"
xprop -id "$spaces" _NET_WM_STATE | grep -q '_NET_WM_STATE_SKIP_TASKBAR'
xwd -root -silent -out /var/lib/companion/native-stage/evidence/heurism-dell-spaces.xwd
xdotool mousemove --window "$spaces" 865 230 click 1
sleep 1
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 1$'
test "$(xdotool getactivewindow)" = "$terminal"
xdotool key --clearmodifiers super+1
sleep 1
xdotool key --clearmodifiers super+o
sleep 1
spaces=$(xdotool search --name '^Heurism Spaces$' | tail -n 1)
test -n "$spaces"
xdotool windowactivate --sync "$spaces"
xdotool key Escape
sleep 1
if xdotool search --name '^Heurism Spaces$' >/dev/null 2>&1; then exit 1; fi
xdotool key --clearmodifiers super+o
sleep 1
spaces=$(xdotool search --name '^Heurism Spaces$' | tail -n 1)
test -n "$spaces"
xdotool mousemove --window "$spaces" 1000 238 click 1
sleep 2
geometry=$(xdotool getwindowgeometry --shell "$terminal")
x=$(printf '%s\n' "$geometry" | sed -n 's/^X=//p')
y=$(printf '%s\n' "$geometry" | sed -n 's/^Y=//p')
width=$(printf '%s\n' "$geometry" | sed -n 's/^WIDTH=//p')
height=$(printf '%s\n' "$geometry" | sed -n 's/^HEIGHT=//p')
test "$x" -ge 0 && test "$x" -lt 100
test "$width" -gt 800 && test "$width" -lt 1000
test "$y" -ge 82 && test $((y + height + 5)) -le 974
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 1$'
xdotool mousemove 1000 1028 click 1
sleep 1
xprop -root _NET_CURRENT_DESKTOP | grep -q ' = 1$'
test "$(xdotool getactivewindow)" = "$terminal"
xdotool windowactivate --sync "$terminal"
xdotool key --clearmodifiers super+shift+1
sleep 1
xprop -id "$terminal" _NET_WM_DESKTOP | grep -q ' = 0$'
xdotool key --clearmodifiers super+space
sleep 1
launcher=$(xdotool search --name '^Heurism Launcher$' | tail -n 1)
test -n "$launcher"
xdotool windowactivate --sync "$launcher"
xdotool type --clearmodifiers 'orbitnote'
xdotool key Return
sleep 2
xdotool search --name '^Heurism Editor$' >/dev/null
if xdotool search --name '^Heurism Launcher$' >/dev/null 2>&1; then exit 1; fi
test "$(readlink -f /opt/heurism/native/current)" = "$release"
rc-service heurism-desktop status >/dev/null
rc-service heurism-control status >/dev/null
rc-service companion-watch status >/dev/null
rc-service sshd status >/dev/null
echo 'Dell-size isolated C Spaces view, four-workspace switching, window move/tiling, quick controls, local file search and terminal passed; :0 unchanged'

#!/bin/sh
# Exercise the C Dell desktop on Xvfb :1 while the production Xorg :0 stays active.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
stage=/var/lib/companion/native-stage
directory=$(mktemp -d /tmp/heurism-native-ui.XXXXXX)
chmod 755 "$directory"
install -m 755 "$stage/heurism-control" "$stage/heurismctl" \
    "$stage/heurism-desktop" "$directory/"
socket=/tmp/heurism-control-native-ui-$$.sock
state=/tmp/heurism-native-ui-preferences-$$.json
cleanup() {
    if [ "$ui" -gt 0 ]; then kill "$ui" 2>/dev/null || true; wait "$ui" 2>/dev/null || true; fi
    kill "$control" "$xvfb" 2>/dev/null || true
    wait "$control" "$xvfb" 2>/dev/null || true
    rm -f "$socket" "$state"
    rm -f "$directory/heurism-control" "$directory/heurismctl" "$directory/heurism-desktop" "$directory"/*.log
    rmdir "$directory"
}
Xvfb :1 -screen 0 1920x1080x24 -nolisten tcp >"$directory/xvfb.log" 2>&1 &
xvfb=$!
"$directory/heurism-control" --socket "$socket" --state "$state" \
    >"$directory/control.log" 2>&1 &
control=$!
ui=0
trap cleanup EXIT HUP INT TERM
attempt=0
while ! DISPLAY=:1 xdotool getdisplaygeometry >/dev/null 2>&1 || [ ! -S "$socket" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 20
    sleep 1
done
su -s /bin/sh -c "DISPLAY=:1 HOME=/var/lib/companion/desktop-user $directory/heurism-desktop --socket $socket" companion-ui \
    >"$directory/ui.log" 2>&1 &
ui=$!
attempt=0
until DISPLAY=:1 xdotool search --name 'Heurism dock' >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 15
    kill -0 "$ui"
    sleep 1
done
desktop=$(DISPLAY=:1 xdotool search --name '^Heurism desktop$' | tail -n 1)
test -n "$desktop"
DISPLAY=:1 xdotool key --window "$desktop" F2
sleep 1
DISPLAY=:1 xdotool mousemove 170 365 click 1
sleep 1
case "$(cat "$state")" in *'"theme":"light"'*) ;; *) echo 'Appearance click did not reach C control' >&2; exit 1 ;; esac
DISPLAY=:1 xwd -root -silent -out /tmp/heurism-native-dell-settings.xwd
DISPLAY=:1 xdotool key --window "$desktop" F3
sleep 1
kill -0 "$ui"
DISPLAY=:1 xwd -root -silent -out /tmp/heurism-native-dell-device.xwd
DISPLAY=:1 xdotool key --window "$desktop" F5
sleep 1
DISPLAY=:1 xdotool mousemove 145 404 click 1
sleep 10
kill -0 "$ui"
DISPLAY=:1 xdotool mousemove 200 735 click 1
sleep 1
DISPLAY=:1 xdotool type --clearmodifiers 'test-only-pass'
sleep 1
DISPLAY=:1 xwd -root -silent -out /tmp/heurism-native-dell-network.xwd
DISPLAY=:1 xdotool key --window "$desktop" F6
sleep 1
kill -0 "$ui"
DISPLAY=:1 xwd -root -silent -out /tmp/heurism-native-dell-sound.xwd
echo 'Dell native desktop Xvfb settings, device, network and sound passed'

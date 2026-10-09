#!/bin/sh
# Live UID-1000 Files/Editor interaction on isolated Xvfb :2.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
stage=/var/lib/companion/native-stage
directory=$(mktemp -d /tmp/companion-native-apps.XXXXXX)
workspace=$(mktemp -d /tmp/companion-native-docs.XXXXXX)
chmod 755 "$directory"
chown companion-ui:companion-ui "$workspace"
install -m 755 "$stage/companion-app" "$directory/companion-app"
install -m 755 "$stage/companion-terminal" "$directory/companion-terminal"
ln "$directory/companion-app" "$directory/companion-files"
ln "$directory/companion-app" "$directory/companion-editor"
install -m 644 "$stage/openbox.xml" "$directory/openbox.xml"
cleanup() {
    DISPLAY=:2 xdotool search --name '^Companion Files$' windowclose 2>/dev/null || true
    DISPLAY=:2 xdotool search --name '^Companion Editor$' windowclose 2>/dev/null || true
    if [ "$openbox" -gt 0 ]; then kill "$openbox" 2>/dev/null || true; wait "$openbox" 2>/dev/null || true; fi
    kill "$xvfb" 2>/dev/null || true
    wait "$xvfb" 2>/dev/null || true
    case "$directory:$workspace" in /tmp/companion-native-apps.*:/tmp/companion-native-docs.*) ;;
        *) exit 1 ;;
    esac
    rm -rf "$directory" "$workspace"
}
Xvfb :2 -screen 0 1280x800x24 -nolisten tcp >"$directory/xvfb.log" 2>&1 &
xvfb=$!
openbox=0
trap cleanup EXIT HUP INT TERM
attempt=0
until DISPLAY=:2 xdotool getdisplaygeometry >/dev/null 2>&1; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 20
    sleep 1
done
su -s /bin/sh -c "DISPLAY=:2 HOME=/var/lib/companion/desktop-user exec openbox --config-file $directory/openbox.xml" companion-ui \
    >"$directory/openbox.log" 2>&1 &
openbox=$!
attempt=0
until DISPLAY=:2 xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q 'window id'; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 15
    kill -0 "$openbox"
    sleep 1
done
su -s /bin/sh -c "printf start >$workspace/seed.txt" companion-ui
su -s /bin/sh -c "DISPLAY=:2 HOME=/var/lib/companion/desktop-user exec $directory/companion-files $workspace" companion-ui \
    >"$directory/files.log" 2>&1 &
attempt=0
until files=$(DISPLAY=:2 xdotool search --name '^Companion Files$' | tail -n 1) && [ -n "$files" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 15
    sleep 1
done
DISPLAY=:2 xdotool windowactivate --sync "$files"
DISPLAY=:2 xdotool mousemove --window "$files" 280 78 click 1
DISPLAY=:2 xdotool type --clearmodifiers --window "$files" created
DISPLAY=:2 xdotool key --window "$files" Return
sleep 1
test -d "$workspace/created"
su -s /bin/sh -c "DISPLAY=:2 HOME=/var/lib/companion/desktop-user exec $directory/companion-editor $workspace/seed.txt" companion-ui \
    >"$directory/editor.log" 2>&1 &
attempt=0
until editor=$(DISPLAY=:2 xdotool search --name '^Companion Editor$' | tail -n 1) && [ -n "$editor" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 15
    sleep 1
done
DISPLAY=:2 xdotool windowactivate --sync "$editor"
DISPLAY=:2 xdotool type --clearmodifiers --window "$editor" 'native '
DISPLAY=:2 xdotool key --window "$editor" ctrl+s
sleep 1
test "$(cat "$workspace/seed.txt")" = 'native start'
test "$(stat -c %u "$workspace/seed.txt")" = 1000
echo 'Dell native Files and Editor checks passed'

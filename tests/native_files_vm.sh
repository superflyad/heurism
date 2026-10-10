#!/bin/sh
# Exercise C Files navigation and file associations on an isolated VM display.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
app=${1:-/var/lib/companion/native-stage/heurism-app}
test -x "$app" && test ! -L "$app"
for program in Xvfb xdotool xwd dbus-run-session base64 xfwm4 xfconf-query tar; do
    command -v "$program" >/dev/null
done
test ! -e /tmp/.X92-lock
work=$(mktemp -d /tmp/heurism-files-test.XXXXXX)
chown companion-ui:companion-ui "$work"
chmod 700 "$work"
install -d -o companion-ui -g companion-ui -m 700 \
    "$work/docs" "$work/config" "$work/run"
install -d -o companion-ui -g companion-ui -m 755 "$work/data/applications"
install -d -o companion-ui -g companion-ui -m 755 "$work/.themes/Heurism/xfwm4"
tar -xzf /opt/heurism/native/current/heurism-xfwm4.tar.gz \
    -C "$work/.themes/Heurism/xfwm4"
chown -R companion-ui:companion-ui "$work/.themes"
install -m 755 "$app" "$work/heurism-app"
ln "$work/heurism-app" "$work/heurism-files"
ln "$work/heurism-app" "$work/heurism-editor"
printf 'hello\n' >"$work/docs/seed.txt"
printf 'hidden\n' >"$work/docs/.secret"
printf '%s' 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jRZkAAAAASUVORK5CYII=' |
    base64 -d >"$work/docs/image.png"
cat >"$work/data/applications/heurism-fixture-image.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Fixture Image Viewer
Exec=/bin/touch $work/image-opened
MimeType=image/png;
NoDisplay=true
EOF
cat >"$work/config/mimeapps.list" <<EOF
[Default Applications]
image/png=heurism-fixture-image.desktop
EOF
chown companion-ui:companion-ui "$work/docs/seed.txt" "$work/docs/.secret" \
    "$work/docs/image.png" "$work/config/mimeapps.list" \
    "$work/data/applications/heurism-fixture-image.desktop"
Xvfb :92 -screen 0 1280x800x24 -nolisten tcp -ac >"$work/xvfb.log" 2>&1 &
xvfb=$!
session=0
cleanup() {
    if [ "$session" -ne 0 ]; then kill "$session" 2>/dev/null || true; fi
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
su -s /bin/sh -c "HOME=$work XDG_CONFIG_HOME=$work/config XDG_DATA_HOME=$work/data XDG_RUNTIME_DIR=$work/run DISPLAY=:92 XAUTHORITY=/dev/null dbus-run-session sh -c 'xfconf-query -c xfwm4 -p /general/theme -n -t string -s Heurism; xfwm4 >$work/xfwm.log 2>&1 & exec $work/heurism-files $work/docs'" companion-ui \
    >"$work/files.log" 2>&1 &
session=$!
sleep 2
export DISPLAY=:92 XAUTHORITY=/dev/null
files=$(xdotool search --name '^Heurism Files$' | tail -n 1)
test -n "$files"
xdotool windowactivate --sync "$files"
xwd -id "$files" -silent -out "$work/files-initial.xwd"
xdotool key --window "$files" ctrl+shift+n
xdotool type --clearmodifiers --window "$files" created
xdotool key --window "$files" Return
sleep 1
test -d "$work/docs/created"
xdotool mousemove --window "$files" 305 219 click 1
xwd -id "$files" -silent -out "$work/files-selected.xwd"
xdotool mousemove --window "$files" 687 569 click 1
xdotool key --window "$files" ctrl+a
xdotool type --clearmodifiers --window "$files" renamed
xdotool key --window "$files" Return
sleep 1
test -d "$work/docs/renamed"
xdotool mousemove --window "$files" 305 219 click 1
xdotool mousemove --window "$files" 805 569 click 1
xdotool type --clearmodifiers --window "$files" yes
xdotool key --window "$files" Return
sleep 1
test ! -e "$work/docs/renamed"
xdotool mousemove --window "$files" 85 370 click 1
xdotool mousemove --window "$files" 305 219 click 1
xdotool mousemove --window "$files" 805 569 click 1
sleep 1
test -d "$work/docs/renamed"
xdotool key --window "$files" ctrl+l
xdotool key --window "$files" ctrl+a
xdotool type --clearmodifiers --window "$files" "$work/docs"
xdotool key --window "$files" Return
sleep 1
xdotool mousemove --window "$files" 305 260 click 1
xdotool key --window "$files" Return
sleep 2
test -f "$work/image-opened"
test "$(stat -c %u "$work/image-opened")" = "$(id -u companion-ui)"
xdotool mousemove --window "$files" 305 302 click 1
xdotool key --window "$files" Return
sleep 1
xdotool search --name '^Heurism Editor$' >/dev/null
xdotool windowactivate --sync "$files"
xdotool key --window "$files" ctrl+h
sleep 1
xwd -id "$files" -silent -out "$work/files-hidden.xwd"
xdotool mousemove --window "$files" 85 196 click 1
sleep 1
xdotool key --window "$files" ctrl+l
xdotool key --window "$files" ctrl+a
xdotool type --clearmodifiers --window "$files" "$work/docs"
xdotool key --window "$files" Return
sleep 1
xwd -id "$files" -silent -out "$work/files-returned.xwd"
echo "C Files create, rename, trash, restore, associations, hidden toggle and navigation passed; captures: $work/files-initial.xwd $work/files-selected.xwd $work/files-hidden.xwd $work/files-returned.xwd"

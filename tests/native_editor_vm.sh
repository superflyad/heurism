#!/bin/sh
# Check C Editor interaction and rendering on an isolated VM X server.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
app=${1:-/var/lib/companion/native-stage/heurism-app}
test -x "$app" && test ! -L "$app"
for program in Xvfb xfwm4 xfconf-query xdotool xwd dbus-run-session tar; do
    command -v "$program" >/dev/null
done
test ! -e /tmp/.X94-lock
work=$(mktemp -d /tmp/heurism-editor-test.XXXXXX)
chown companion-ui:companion-ui "$work"
chmod 700 "$work"
install -d -o companion-ui -g companion-ui -m 700 "$work/run" "$work/Documents"
install -d -o companion-ui -g companion-ui -m 755 "$work/.themes/Heurism/xfwm4"
tar -xzf /opt/heurism/native/current/heurism-xfwm4.tar.gz \
    -C "$work/.themes/Heurism/xfwm4"
chown -R companion-ui:companion-ui "$work/.themes"
install -m 755 "$app" "$work/heurism-editor"
printf 'first\nsecond\n' >"$work/Documents/note.txt"
printf 'other\n' >"$work/Documents/other.txt"
chown companion-ui:companion-ui "$work/Documents/note.txt" "$work/Documents/other.txt"
Xvfb :94 -screen 0 1280x800x24 -nolisten tcp -ac >"$work/xvfb.log" 2>&1 &
xvfb=$!
session=0
wm=0
reopened=0
other_process=0
copy_process=0 paste_process=0
cleanup() {
    test "$session" = 0 || kill "$session" 2>/dev/null || true
    test "$reopened" = 0 || kill "$reopened" 2>/dev/null || true
    test "$other_process" = 0 || kill "$other_process" 2>/dev/null || true
    test "$copy_process" = 0 || kill "$copy_process" 2>/dev/null || true
    test "$paste_process" = 0 || kill "$paste_process" 2>/dev/null || true
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
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:94 XAUTHORITY=/dev/null dbus-run-session sh -c 'xfconf-query -c xfwm4 -p /general/theme -n -t string -s Heurism; exec xfwm4'" companion-ui \
    >"$work/xfwm.log" 2>&1 &
wm=$!
sleep 3
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:94 XAUTHORITY=/dev/null exec $work/heurism-editor $work/Documents/note.txt" companion-ui \
    >"$work/editor.log" 2>&1 &
session=$!
sleep 2
export DISPLAY=:94 XAUTHORITY=/dev/null
editor=$(xdotool search --name '^Heurism Editor$' | tail -n 1)
test -n "$editor"
xdotool windowactivate --sync "$editor"
xwd -id "$editor" -silent -out "$work/editor-initial.xwd"
xdotool mousemove --window "$editor" 300 212 click 1
xdotool type --clearmodifiers --window "$editor" X
xdotool key --window "$editor" ctrl+s
sleep 1
test "$(cat "$work/Documents/note.txt")" = "firstX
second"
xdotool key --window "$editor" ctrl+z
xdotool key --window "$editor" ctrl+s
sleep 1
test "$(cat "$work/Documents/note.txt")" = "first
second"
xdotool key --window "$editor" ctrl+y
xdotool key --window "$editor" ctrl+s
sleep 1
test "$(cat "$work/Documents/note.txt")" = "firstX
second"
xdotool mousemove --window "$editor" 300 242 click 1
xdotool type --clearmodifiers --window "$editor" '!'
xdotool key --window "$editor" ctrl+s
sleep 1
test "$(cat "$work/Documents/note.txt")" = "firstX
second!"
xdotool type --clearmodifiers --window "$editor" \
    '01234567890123456789012345678901234567890123456789012345678901234567890123456789'
sleep 1
xwd -id "$editor" -silent -out "$work/editor-long-line.xwd"
xdotool key --window "$editor" ctrl+z
xdotool key --window "$editor" ctrl+s
sleep 1
test "$(cat "$work/Documents/note.txt")" = "firstX
second!"
xdotool type --clearmodifiers --window "$editor" draft
sleep 3
set -- "$work"/.local/state/heurism/drafts/draft-*.json
test "$#" = 1 && test -f "$1"
draft=$1
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:94 XAUTHORITY=/dev/null exec $work/heurism-editor $work/Documents/other.txt" companion-ui \
    >"$work/other.log" 2>&1 &
other_process=$!
sleep 2
other_editor=$(xdotool search --name '^Heurism Editor$' | tail -n 1)
test -n "$other_editor" && test "$other_editor" != "$editor"
xdotool type --clearmodifiers --window "$other_editor" X
sleep 3
set -- "$work"/.local/state/heurism/drafts/draft-*.json
test "$#" = 2 && test -f "$1" && test -f "$2"
xdotool key --window "$other_editor" ctrl+s
sleep 1
test "$(cat "$work/Documents/other.txt")" = Xother
xdotool windowclose "$other_editor"
wait "$other_process"
other_process=0
test -f "$draft"
xdotool windowclose "$editor"
wait "$session"
session=0
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:94 XAUTHORITY=/dev/null exec $work/heurism-editor" companion-ui \
    >"$work/reopened.log" 2>&1 &
reopened=$!
sleep 2
recovered=$(xdotool search --name '^Heurism Editor$' | tail -n 1)
test -n "$recovered"
xdotool key --window "$recovered" ctrl+s
sleep 1
test "$(cat "$work/Documents/note.txt")" = "firstX
second!draft"
test ! -e "$draft"
xdotool windowclose "$recovered"
wait "$reopened"
reopened=0
legacy="$work/.local/state/heurism/native-editor-draft.json"
printf '{"path":"%s","text":"legacy text"}\n' "$work/Documents/note.txt" >"$legacy"
chown companion-ui:companion-ui "$legacy"
chmod 600 "$legacy"
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:94 XAUTHORITY=/dev/null exec $work/heurism-editor" companion-ui \
    >"$work/legacy.log" 2>&1 &
reopened=$!
sleep 2
recovered=$(xdotool search --name '^Heurism Editor$' | tail -n 1)
test -n "$recovered"
xdotool key --window "$recovered" ctrl+s
sleep 1
test "$(cat "$work/Documents/note.txt")" = 'legacy text'
test ! -e "$legacy"
xdotool windowclose "$recovered"
wait "$reopened"
reopened=0
printf 'alpha café\nline two\n' >"$work/Documents/copy.txt"
printf 'target\n' >"$work/Documents/paste.txt"
chown companion-ui:companion-ui "$work/Documents/copy.txt" "$work/Documents/paste.txt"
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:94 XAUTHORITY=/dev/null exec $work/heurism-editor $work/Documents/copy.txt" companion-ui \
    >"$work/copy.log" 2>&1 &
copy_process=$!
sleep 2
copy_editor=$(xdotool search --name '^Heurism Editor$' | tail -n 1)
test -n "$copy_editor"
xdotool windowactivate --sync "$copy_editor"
xdotool key --window "$copy_editor" ctrl+a
xwd -id "$copy_editor" -silent -out "$work/editor-selection.xwd"
xdotool key --window "$copy_editor" ctrl+c
su -s /bin/sh -c "HOME=$work XDG_RUNTIME_DIR=$work/run DISPLAY=:94 XAUTHORITY=/dev/null exec $work/heurism-editor $work/Documents/paste.txt" companion-ui \
    >"$work/paste.log" 2>&1 &
paste_process=$!
sleep 2
paste_editor=$(xdotool search --name '^Heurism Editor$' | tail -n 1)
test -n "$paste_editor" && test "$paste_editor" != "$copy_editor"
xdotool windowactivate --sync "$paste_editor"
xdotool key --window "$paste_editor" ctrl+End ctrl+v
sleep 1
xwd -id "$paste_editor" -silent -out "$work/editor-paste-check.xwd"
xdotool key --window "$paste_editor" ctrl+s
sleep 1
printf 'target\nalpha café\nline two\n' >"$work/expected.txt"
cmp "$work/expected.txt" "$work/Documents/paste.txt"
xdotool windowactivate --sync "$copy_editor"
xdotool key --window "$copy_editor" ctrl+a ctrl+x ctrl+s
sleep 1
test ! -s "$work/Documents/copy.txt"
xdotool key --window "$copy_editor" ctrl+z ctrl+s
sleep 1
printf 'alpha café\nline two\n' >"$work/expected-clipboard.txt"
cmp "$work/expected-clipboard.txt" "$work/Documents/copy.txt"
xdotool key --window "$copy_editor" ctrl+y ctrl+s
sleep 1
test ! -s "$work/Documents/copy.txt"
if command -v xclip >/dev/null; then
    timeout 3 xclip -selection clipboard -o >"$work/cut-clipboard.txt"
    cmp "$work/expected-clipboard.txt" "$work/cut-clipboard.txt"
fi
xdotool windowactivate --sync "$paste_editor"
xdotool key --window "$paste_editor" ctrl+End ctrl+v
sleep 1
xwd -id "$paste_editor" -silent -out "$work/editor-second-paste.xwd"
xdotool key --window "$paste_editor" ctrl+s
sleep 1
printf 'target\nalpha café\nline two\nalpha café\nline two\n' >"$work/expected.txt"
cmp "$work/expected.txt" "$work/Documents/paste.txt"
xdotool windowactivate --sync "$copy_editor"
xdotool type --clearmodifiers --window "$copy_editor" abcdef
if command -v xclip >/dev/null; then
    xdotool mousemove --window "$copy_editor" 84 212 mousedown 1 \
        mousemove --window "$copy_editor" 128 212 mouseup 1
    selected=$(timeout 3 xclip -selection primary -o)
    case "$selected" in a|ab|abc|abcd|abcde|abcdef) ;; *) exit 1 ;; esac
fi
xdotool key --window "$copy_editor" ctrl+Home shift+Right shift+Right shift+Right ctrl+c
xdotool type --clearmodifiers --window "$copy_editor" Z
xdotool key --window "$copy_editor" ctrl+s
sleep 1
test "$(cat "$work/Documents/copy.txt")" = Zdef
xdotool key --window "$copy_editor" ctrl+z ctrl+s
sleep 1
test "$(cat "$work/Documents/copy.txt")" = abcdef
xdotool windowactivate --sync "$paste_editor"
xdotool key --window "$paste_editor" ctrl+End ctrl+v
sleep 1
xdotool key --window "$paste_editor" ctrl+s
sleep 1
printf 'target\nalpha café\nline two\nalpha café\nline two\nabc' >"$work/expected.txt"
cmp "$work/expected.txt" "$work/Documents/paste.txt"
if command -v xclip >/dev/null; then
    dd if=/dev/zero bs=1024 count=320 2>/dev/null | tr '\000' Q >"$work/large.txt"
    xclip -quiet -loops 1 -selection clipboard -i "$work/large.txt" \
        >"$work/xclip.log" 2>&1 &
    clipboard_process=$!
    sleep 1
    xdotool key --window "$paste_editor" ctrl+End ctrl+v
    sleep 3
    xdotool key --window "$paste_editor" ctrl+s
    sleep 1
    cat "$work/expected.txt" "$work/large.txt" >"$work/large-expected.txt"
    cmp "$work/large-expected.txt" "$work/Documents/paste.txt"
    wait "$clipboard_process"
    xdotool key --window "$paste_editor" ctrl+a ctrl+c
    timeout 5 xclip -selection clipboard -o >"$work/large-copied.txt"
    cmp "$work/large-expected.txt" "$work/large-copied.txt"
fi
xdotool windowclose "$copy_editor"
wait "$copy_process"
copy_process=0
xdotool windowclose "$paste_editor"
wait "$paste_process"
paste_process=0
echo "C Editor pointer caret, save, undo, redo, concurrent drafts, recovery, UTF-8 selection/copy/cut/paste and legacy migration passed; captures: $work/editor-initial.xwd $work/editor-long-line.xwd $work/editor-selection.xwd"

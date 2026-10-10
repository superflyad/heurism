#!/bin/sh
# Exercise Quick Lock with Xfce's PAM screensaver on an isolated VM display.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
test "$(cat /sys/class/dmi/id/product_name)" = 'Virtual Machine'
release=$(readlink -f "${1:-/opt/heurism/native/current}")
case "$release" in /opt/heurism/native/releases/*) ;; *) exit 1 ;; esac
"$release/heurism-release" verify "$release" >/dev/null
for program in Xvfb xfwm4 xfce4-screensaver xfce4-screensaver-command \
    xdotool dbus-run-session; do command -v "$program" >/dev/null; done
test ! -e /tmp/.X92-lock
work=$(mktemp -d /tmp/heurism-quick-lock.XXXXXX)
chown companion-ui:companion-ui "$work"
chmod 700 "$work"
install -d -o companion-ui -g companion-ui -m 700 "$work/run"
install -d -o companion-ui -g companion-ui -m 755 \
    "$work/.themes" "$work/.themes/Heurism" "$work/.themes/Heurism/xfwm4"
su -s /bin/sh -c "tar -xzf '$release/heurism-xfwm4.tar.gz' -C '$work/.themes/Heurism/xfwm4'" companion-ui
cat >"$work/session.sh" <<'SESSION'
#!/bin/sh
set -eu
printf '%s\n' "$DBUS_SESSION_BUS_ADDRESS" >"$HEURISM_TEST_WORK/bus-address"
xfwm4 >"$HEURISM_TEST_WORK/xfwm.log" 2>&1 &
xfce4-screensaver >"$HEURISM_TEST_WORK/screensaver.log" 2>&1 &
"$HEURISM_TEST_RELEASE/heurism-desktop" --quick >"$HEURISM_TEST_WORK/quick.log" 2>&1
sleep 12
SESSION
chown companion-ui:companion-ui "$work/session.sh"
chmod 700 "$work/session.sh"
Xvfb :92 -screen 0 1280x800x24 -nolisten tcp -ac >"$work/xvfb.log" 2>&1 &
xvfb=$!
session=0
cleanup() {
    test "$session" = 0 || kill "$session" 2>/dev/null || true
    for environment in /proc/[0-9]*/environ; do
        test -r "$environment" || continue
        pid=${environment#/proc/}
        pid=${pid%/environ}
        test "$(stat -c %u "/proc/$pid" 2>/dev/null || true)" = "$(id -u companion-ui)" || continue
        if cat "$environment" 2>/dev/null | tr '\000' '\n' |
           grep -Fxq "XDG_RUNTIME_DIR=$work/run"; then kill "$pid" 2>/dev/null || true; fi
    done
    kill "$xvfb" 2>/dev/null || true
    wait "$xvfb" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM
sleep 2
su -s /bin/sh -c "HOME=$work DISPLAY=:92 XAUTHORITY=/dev/null XDG_RUNTIME_DIR=$work/run HEURISM_TEST_WORK=$work HEURISM_TEST_RELEASE=$release exec dbus-run-session sh '$work/session.sh'" companion-ui >"$work/session.log" 2>&1 &
session=$!
sleep 5
quick=$(DISPLAY=:92 XAUTHORITY=/dev/null xdotool search --name '^Heurism Quick Controls$' | tail -n 1)
test -n "$quick"
DISPLAY=:92 XAUTHORITY=/dev/null xdotool mousemove --window "$quick" 80 460 click 1
sleep 3
address=$(cat "$work/bus-address")
status=$(su -s /bin/sh -c "HOME=$work DISPLAY=:92 XAUTHORITY=/dev/null XDG_RUNTIME_DIR=$work/run DBUS_SESSION_BUS_ADDRESS=$address xfce4-screensaver-command --query" companion-ui)
printf '%s\n' "$status" | grep -Fxq 'The screensaver is active'
echo "Quick Lock activated the PAM screensaver on an isolated display; logs: $work"

#!/bin/sh
set -eu
export DISPLAY=:0
export XAUTHORITY=/run/heurism-desktop/Xauthority
/opt/heurism/native/current/heurism-release verify
mkdir -p /run/heurism-desktop/user /var/lib/companion/desktop-user/.local/state/heurism
for directory in /run/heurism-desktop/user \
    /var/lib/companion/desktop-user/.local \
    /var/lib/companion/desktop-user/.local/state \
    /var/lib/companion/desktop-user/.local/state/heurism; do
    chown companion-ui:companion-ui "$directory"
    chmod 700 "$directory"
done
token=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
test "${#token}" = 32
session_uid=$(id -u companion-ui)
stop_session_processes() {
    signal=$1
    for environment in /proc/[0-9]*/environ; do
        test -r "$environment" || continue
        pid=${environment#/proc/}
        pid=${pid%/environ}
        test "$(stat -c %u "/proc/$pid" 2>/dev/null || true)" = "$session_uid" || continue
        if cat "$environment" 2>/dev/null | tr '\000' '\n' |
           grep -Fxq "HEURISM_SESSION_TOKEN=$token"; then
            kill -"$signal" "$pid" 2>/dev/null || true
        fi
    done
}
cleanup() {
    trap - EXIT
    kill "${client:-}" 2>/dev/null || true
    wait "${client:-}" 2>/dev/null || true
    stop_session_processes TERM
    sleep 1
    stop_session_processes KILL
}
trap cleanup EXIT
trap 'exit 143' TERM INT HUP
select_xfce_recovery() {
    if [ "$(cat /etc/companion/native-session-mode 2>/dev/null || true)" != heurism ]; then
        return
    fi
    # A failed C workspace or locker must leave an accessible, locked desktop
    # on the next supervised Xorg start. Only this root client changes mode.
    temporary=/etc/companion/native-session-mode.recovery.$$
    printf 'xfce\n' >"$temporary"
    chmod 644 "$temporary"
    mv -f "$temporary" /etc/companion/native-session-mode
    echo 'Heurism workspace failed; selected Xfce recovery session' >&2
}
su -s /bin/sh -c "export XDG_RUNTIME_DIR=/run/heurism-desktop/user HEURISM_SESSION_TOKEN=$token; exec dbus-run-session /opt/heurism/native/current/user-session.sh" companion-ui &
client=$!
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    if /opt/heurism/native/current/heurism-release health >/dev/null 2>&1; then
        ready=1
        break
    fi
    kill -0 "$client" 2>/dev/null || break
    sleep 1
done
if [ "$ready" = 0 ]; then
    echo 'Native UI did not become healthy' >&2
    : > /run/heurism-desktop/startup-failed
    kill "$client" 2>/dev/null || true
    wait "$client" 2>/dev/null || true
    select_xfce_recovery
    exit 1
fi
if wait "$client"; then
    exit 0
else
    result=$?
fi
select_xfce_recovery
exit "$result"

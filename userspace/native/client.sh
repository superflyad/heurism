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
su -s /bin/sh -c 'export XDG_RUNTIME_DIR=/run/heurism-desktop/user; exec dbus-run-session /opt/heurism/native/current/user-session.sh' companion-ui &
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
    exit 1
fi
wait "$client"

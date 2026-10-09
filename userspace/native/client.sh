#!/bin/sh
set -eu
export DISPLAY=:0
export XAUTHORITY=/run/companion-desktop/Xauthority
/opt/companion/native/current/companion-release verify
mkdir -p /run/companion-desktop/user /var/lib/companion/desktop-user/.local/state/companion
for directory in /run/companion-desktop/user \
    /var/lib/companion/desktop-user/.local \
    /var/lib/companion/desktop-user/.local/state \
    /var/lib/companion/desktop-user/.local/state/companion; do
    chown companion-ui:companion-ui "$directory"
    chmod 700 "$directory"
done
su -s /bin/sh -c 'export XDG_RUNTIME_DIR=/run/companion-desktop/user; exec dbus-run-session /opt/companion/native/current/user-session.sh' companion-ui &
client=$!
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    if /opt/companion/native/current/companion-release health >/dev/null 2>&1; then
        ready=1
        break
    fi
    kill -0 "$client" 2>/dev/null || break
    sleep 1
done
if [ "$ready" = 0 ]; then
    echo 'Native UI did not become healthy' >&2
    kill "$client" 2>/dev/null || true
    wait "$client" 2>/dev/null || true
    exit 1
fi
wait "$client"

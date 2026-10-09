#!/bin/sh
# Root X startup. The legacy release remains a bounded recovery fallback.
set -eu
export DISPLAY=:0
export XAUTHORITY=/run/companion-desktop/Xauthority
mkdir -p /run/companion-desktop
if /opt/companion/native/current/companion-session-config; then
    umask 077
    cookie=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
    touch "$XAUTHORITY"
    xauth -f "$XAUTHORITY" add :0 . "$cookie"
    chown root:companion-ui "$XAUTHORITY"
    chmod 640 "$XAUTHORITY"
    xinit /opt/companion/native/current/client.sh -- /usr/bin/Xorg :0 vt7 \
        -config /run/companion-desktop/xorg.conf -nolisten tcp -auth "$XAUTHORITY" \
        -logfile /var/log/companion-xorg.log &
    server=$!
    stop() {
        trap - TERM INT HUP
        kill "$server" 2>/dev/null || true
        wait "$server" 2>/dev/null || true
        exit 143
    }
    trap stop TERM INT HUP
    if wait "$server"; then
        exit 0
    fi
    trap - TERM INT HUP
fi
echo 'Native session failed; starting sealed legacy recovery release' >&2
exec /opt/companion/desktop/session.sh

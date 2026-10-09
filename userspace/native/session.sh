#!/bin/sh
# Root X startup. The legacy release remains a bounded recovery fallback.
set -eu
export DISPLAY=:0
export XAUTHORITY=/run/heurism-desktop/Xauthority
mkdir -p /run/heurism-desktop
if /opt/heurism/native/current/heurism-session-config; then
    umask 077
    cookie=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
    touch "$XAUTHORITY"
    xauth -f "$XAUTHORITY" add :0 . "$cookie"
    chown root:companion-ui "$XAUTHORITY"
    chmod 640 "$XAUTHORITY"
    rm -f /run/heurism-desktop/startup-failed
    xinit /opt/heurism/native/current/client.sh -- /usr/bin/Xorg :0 vt7 \
        -config /run/heurism-desktop/xorg.conf -nolisten tcp -auth "$XAUTHORITY" \
        -logfile /var/log/heurism-xorg.log &
    server=$!
    stop() {
        trap - TERM INT HUP
        kill "$server" 2>/dev/null || true
        wait "$server" 2>/dev/null || true
        exit 143
    }
    trap stop TERM INT HUP
    if wait "$server"; then result=0; else result=$?; fi
    trap - TERM INT HUP
    if [ ! -e /run/heurism-desktop/startup-failed ]; then exit "$result"; fi
fi
if [ -x /opt/companion/desktop/session.sh ]; then
    echo 'Native session failed; starting sealed legacy recovery release' >&2
    umask 022
    exec /opt/companion/desktop/session.sh
fi
echo 'No legacy graphical recovery release; root SSH/watch remain independent' >&2
exit 1

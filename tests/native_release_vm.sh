#!/bin/sh
# Inspect the sealed native release beside the active legacy VM session.
set -eu
candidate=$(cat /var/lib/companion/native-candidate-release)
case "$candidate" in /opt/heurism/native/releases/heurism-os-*) ;; *) exit 1 ;; esac
"$candidate/heurism-release" verify "$candidate"
socket=/run/heurism-desktop/control-candidate.sock
"$candidate/heurism-control" --socket "$socket" \
    --state /tmp/heurism-native-candidate-preferences.json \
    >/tmp/heurism-native-candidate-control.log 2>&1 &
echo $! >/tmp/heurism-native-candidate-control.pid
attempt=0
while [ ! -S "$socket" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 15
    sleep 1
done
su -s /bin/sh -c "
    export DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority
    export XDG_RUNTIME_DIR=/run/heurism-desktop/user
    nohup $candidate/heurism-desktop --socket $socket \
        >/tmp/heurism-native-candidate-desktop.log 2>&1 </dev/null &
" companion-ui
ready=0
for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    if "$candidate/heurism-release" health "$candidate" >/dev/null 2>&1; then
        ready=1
        break
    fi
    sleep 1
done
test "$ready" = 1
"$candidate/heurism-release" health "$candidate"
echo 'native release and UI health checks passed'

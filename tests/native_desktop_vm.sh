#!/bin/sh
# Launch the candidate C workspace beside the current VM session for inspection.
set -eu
candidate=/opt/heurism/native/candidate
socket=/run/heurism-desktop/control-native-test.sock
install -m 755 /var/lib/companion/native-stage/heurism-desktop "$candidate/"
"$candidate/heurism-control" --socket "$socket" \
    --state /tmp/heurism-native-desktop-preferences.json \
    >/tmp/heurism-native-control.log 2>&1 &
echo $! >/tmp/heurism-native-control.pid
attempt=0
while [ ! -S "$socket" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 15
    sleep 1
done
su -s /bin/sh -c '
    export DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority
    export XDG_RUNTIME_DIR=/run/heurism-desktop/user
    nohup /opt/heurism/native/candidate/heurism-desktop \
        --socket /run/heurism-desktop/control-native-test.sock \
        >/tmp/heurism-native-desktop.log 2>&1 </dev/null &
' companion-ui
sleep 2
su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool search --name "^Heurism desktop$"' companion-ui
su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool search --name "^Heurism dock$"' companion-ui
echo 'native desktop windows ready'

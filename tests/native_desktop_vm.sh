#!/bin/sh
# Launch the candidate C workspace beside the current VM session for inspection.
set -eu
candidate=/opt/companion/native/candidate
socket=/run/companion-desktop/control-native-test.sock
install -m 755 /var/lib/companion/native-stage/companion-desktop "$candidate/"
"$candidate/companion-control" --socket "$socket" \
    --state /tmp/companion-native-desktop-preferences.json \
    >/tmp/companion-native-control.log 2>&1 &
echo $! >/tmp/companion-native-control.pid
attempt=0
while [ ! -S "$socket" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 15
    sleep 1
done
su -s /bin/sh -c '
    export DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority
    export XDG_RUNTIME_DIR=/run/companion-desktop/user
    nohup /opt/companion/native/candidate/companion-desktop \
        --socket /run/companion-desktop/control-native-test.sock \
        >/tmp/companion-native-desktop.log 2>&1 </dev/null &
' companion-ui
sleep 2
su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool search --name "^Companion desktop$"' companion-ui
su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool search --name "^Companion dock$"' companion-ui
echo 'native desktop windows ready'

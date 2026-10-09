#!/bin/sh
# Run only in the CompanionDev VM with its active X session.
set -eu
rm -f /tmp/companion-native-uid /tmp/companion-native-output \
    /tmp/companion-native-size /tmp/companion-native-interrupt \
    /tmp/companion-native-prompt
su -s /bin/sh -c '
    export DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority
    export XDG_RUNTIME_DIR=/run/companion-desktop/user
    nohup /opt/companion/native/current/companion-terminal \
        >/tmp/companion-native-terminal.log 2>&1 </dev/null &
' companion-ui
sleep 2
window=$(su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool search --name "Companion C Terminal"' companion-ui | tail -n 1)
test -n "$window"
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool windowactivate --sync $window" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'id -u > /tmp/companion-native-uid'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/companion-native-uid)" = 1000
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'printf hello | tr a-z A-Z > /tmp/companion-native-output'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/companion-native-output)" = HELLO
test "$(stat -c %u /tmp/companion-native-output)" = 1000
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool windowsize $window 760 500" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'stty size > /tmp/companion-native-size'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
set -- $(cat /tmp/companion-native-size)
test "$1" -ge 5
test "$2" -ge 20
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'sleep 8'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window ctrl+c" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'printf interrupted > /tmp/companion-native-interrupt'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/companion-native-interrupt)" = interrupted
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window ctrl+c" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'printf ready > /tmp/companion-native-prompt'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/companion-native-prompt)" = ready
echo 'native terminal PTY input/output checks passed'

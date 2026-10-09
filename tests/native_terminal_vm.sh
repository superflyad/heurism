#!/bin/sh
# Run only in the CompanionDev VM with its active X session.
set -eu
rm -f /tmp/heurism-native-uid /tmp/heurism-native-output \
    /tmp/heurism-native-size /tmp/heurism-native-interrupt \
    /tmp/heurism-native-prompt
su -s /bin/sh -c '
    export DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority
    export XDG_RUNTIME_DIR=/run/heurism-desktop/user
    nohup /opt/heurism/native/current/heurism-terminal \
        >/tmp/heurism-native-terminal.log 2>&1 </dev/null &
' companion-ui
sleep 2
window=$(su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool search --name "Heurism C Terminal"' companion-ui | tail -n 1)
test -n "$window"
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool windowactivate --sync $window" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'id -u > /tmp/heurism-native-uid'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/heurism-native-uid)" = 1000
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'printf hello | tr a-z A-Z > /tmp/heurism-native-output'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/heurism-native-output)" = HELLO
test "$(stat -c %u /tmp/heurism-native-output)" = 1000
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool windowsize $window 760 500" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'stty size > /tmp/heurism-native-size'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
set -- $(cat /tmp/heurism-native-size)
test "$1" -ge 5
test "$2" -ge 20
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'sleep 8'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window ctrl+c" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'printf interrupted > /tmp/heurism-native-interrupt'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/heurism-native-interrupt)" = interrupted
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window ctrl+c" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool type --clearmodifiers --delay 30 --window $window 'printf ready > /tmp/heurism-native-prompt'" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/heurism-desktop/Xauthority xdotool key --window $window Return" companion-ui
sleep 1
test "$(cat /tmp/heurism-native-prompt)" = ready
echo 'native terminal PTY input/output checks passed'

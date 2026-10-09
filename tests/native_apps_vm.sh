#!/bin/sh
# Live X11 checks on disposable UID-1000 files in CompanionDev.
set -eu
candidate=/opt/companion/native/candidate
root=/tmp/companion-native-apps-test
su -s /bin/sh -c 'mkdir -p /tmp/companion-native-apps-test; printf start > /tmp/companion-native-apps-test/seed.txt' companion-ui
su -s /bin/sh -c '
    export DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority
    export XDG_RUNTIME_DIR=/run/companion-desktop/user
    nohup /opt/companion/native/candidate/companion-files /tmp/companion-native-apps-test \
        >/tmp/companion-native-files.log 2>&1 </dev/null &
' companion-ui
sleep 2
files=$(su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool search --name "^Companion Files$"' companion-ui | tail -n 1)
test -n "$files"
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool windowactivate --sync $files" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 280 78 click 1" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --window $files created" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $files Return" companion-ui
sleep 1
test -d "$root/created"
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 190 211 click 1" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 390 78 click 1" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $files ctrl+a" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --window $files renamed.txt" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $files Return" companion-ui
sleep 1
test -f "$root/renamed.txt"
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 190 211 click 1" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 480 78 click 1" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --window $files yes" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $files Return" companion-ui
sleep 1
test ! -e "$root/renamed.txt"
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 570 78 click 1" companion-ui
sleep 1
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 190 178 click 1" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool mousemove --window $files 660 78 click 1" companion-ui
sleep 1
test -f "$root/renamed.txt"
su -s /bin/sh -c '
    export DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority
    export XDG_RUNTIME_DIR=/run/companion-desktop/user
    nohup /opt/companion/native/candidate/companion-editor /tmp/companion-native-apps-test/renamed.txt \
        >/tmp/companion-native-editor.log 2>&1 </dev/null &
' companion-ui
sleep 2
editor=$(su -s /bin/sh -c 'DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool search --name "^Companion Editor$"' companion-ui | tail -n 1)
test -n "$editor"
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool windowactivate --sync $editor" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool type --clearmodifiers --window $editor 'native '" companion-ui
su -s /bin/sh -c "DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xdotool key --window $editor ctrl+s" companion-ui
sleep 1
test "$(cat "$root/renamed.txt")" = 'native start'
test "$(stat -c %u "$root/renamed.txt")" = 1000
echo 'native Files and Editor checks passed'

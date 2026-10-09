#!/bin/sh
# Sidecar checks on the trusted Dell; leaves the active control service intact.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
stage=/var/lib/companion/native-stage
directory=$(mktemp -d /tmp/companion-native-control.XXXXXX)
chmod 755 "$directory"
install -m 755 "$stage/companion-control" "$stage/companionctl" "$directory/"
socket=/run/companion-desktop/control-native-dell-$$.sock
state=/tmp/companion-native-dell-preferences-$$.json
audio_left=
audio_right=
audio_muted=
light_original=
"$directory/companion-control" --socket "$socket" --state "$state" \
    >/tmp/companion-native-dell-control.log 2>&1 &
server=$!
cleanup() {
    if [ -n "$light_original" ]; then
        printf '%s\n' "$light_original" >/sys/class/backlight/intel_backlight/brightness || true
    fi
    if [ -n "$audio_left" ] && [ -n "$audio_right" ]; then
        su -s /bin/sh -c "/usr/bin/pactl set-sink-volume @DEFAULT_SINK@ $audio_left $audio_right" companion-ui || true
    fi
    if [ -n "$audio_muted" ]; then
        su -s /bin/sh -c "/usr/bin/pactl set-sink-mute @DEFAULT_SINK@ $audio_muted" companion-ui || true
    fi
    kill "$server" 2>/dev/null || true
    wait "$server" 2>/dev/null || true
    rm -f "$state" "$socket"
    rm -f "$directory/companion-control" "$directory/companionctl" "$directory/admin-rejected.json"
    rmdir "$directory"
}
trap cleanup EXIT HUP INT TERM
attempt=0
while [ ! -S "$socket" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 20
    sleep 1
done
result=$("$directory/companionctl" --socket "$socket" status)
case "$result" in
    *'"ok":true'*'"platform":"dell"'*'"batteries"'*'"brightness"'*) ;;
    *) echo 'Dell native status invalid' >&2; exit 1 ;;
esac
light_original=$(cat /sys/class/backlight/intel_backlight/brightness)
result=$("$directory/companionctl" --socket "$socket" brightness 40)
case "$result" in *'"ok":true'*'"brightness":40'*) ;; *) echo "Backlight readback failed: $result" >&2; exit 1 ;; esac
printf '%s\n' "$light_original" >/sys/class/backlight/intel_backlight/brightness
test "$(cat /sys/class/backlight/intel_backlight/brightness)" = "$light_original"
light_original=
result=$(su -s /bin/sh -c "$directory/companionctl --socket $socket status" companion-ui)
case "$result" in *'"ok":true'*) ;; *) exit 1 ;; esac
if su -s /bin/sh -c "$directory/companionctl --socket $socket admin-console" companion-ui \
    >"$directory/admin-rejected.json"; then exit 1; fi
case "$(cat "$directory/admin-rejected.json")" in
    *'Use authenticated root SSH for administration'*) ;;
    *) echo 'Desktop caller reached root console' >&2; exit 1 ;;
esac
result=$("$directory/companionctl" --socket "$socket" power-check)
case "$result" in *'"ok":true'*'"data":true'*) ;; *) echo "$result"; exit 1 ;; esac
result=$("$directory/companionctl" --socket "$socket" bios-list)
case "$result" in *'"ok":true'*'"FnLock"'*'"writable":true'*) ;; *) echo 'BIOS inventory invalid' >&2; exit 1 ;; esac
fnlock=$(cat /sys/class/firmware-attributes/dell-wmi-sysman/attributes/FnLock/current_value)
result=$("$directory/companionctl" --socket "$socket" bios-set "{\"name\":\"FnLock\",\"value\":\"$fnlock\"}")
case "$result" in *'"ok":true'*'"previous":"'$fnlock'"'*) ;; *) echo "BIOS readback failed: $result" >&2; exit 1 ;; esac
result=$("$directory/companionctl" --socket "$socket" input-status)
case "$result" in *'"ok":true'*'"tap"'*'"natural_scroll"'*'"speed"'*) ;; *) echo "Touchpad status invalid: $result" >&2; exit 1 ;; esac
result=$("$directory/companionctl" --socket "$socket" sound-status)
case "$result" in *'"ok":true'*'Speaker__sink'*'"volume"'*'"muted"'*) ;; *) echo "Speaker status invalid: $result" >&2; exit 1 ;; esac
case "$result" in *'"muted":true'*) audio_muted=1 ;; *'"muted":false'*) audio_muted=0 ;; *) exit 1 ;; esac
result=$("$directory/companionctl" --socket "$socket" sound-settings '{"muted":true}')
case "$result" in *'"ok":true'*'"muted":true'*) ;; *) echo "Speaker mute readback failed: $result" >&2; exit 1 ;; esac
original=$(su -s /bin/sh -c '/usr/bin/pactl get-sink-volume @DEFAULT_SINK@' companion-ui)
audio_left=$(printf '%s\n' "$original" | sed -n 's/.*front-left: \([0-9][0-9]*\) \/.*/\1/p')
audio_right=$(printf '%s\n' "$original" | sed -n 's/.*front-right: \([0-9][0-9]*\) \/.*/\1/p')
case "$audio_left:$audio_right" in *[!0-9:]*|:*|*:) echo 'Cannot restore stereo volume' >&2; exit 1 ;; esac
result=$("$directory/companionctl" --socket "$socket" sound-settings '{"volume_left":20,"volume_right":10}')
case "$result" in *'"ok":true'*'"volume_left":20'*'"volume_right":10'*'"muted":true'*) ;; *) echo "Speaker stereo readback failed: $result" >&2; exit 1 ;; esac
su -s /bin/sh -c "/usr/bin/pactl set-sink-volume @DEFAULT_SINK@ $audio_left $audio_right" companion-ui
su -s /bin/sh -c "/usr/bin/pactl set-sink-mute @DEFAULT_SINK@ $audio_muted" companion-ui
restored=$(su -s /bin/sh -c '/usr/bin/pactl get-sink-volume @DEFAULT_SINK@' companion-ui)
test "$original" = "$restored"
case "$("$directory/companionctl" --socket "$socket" sound-status)" in
    *'"muted":true'*) test "$audio_muted" = 1 ;;
    *'"muted":false'*) test "$audio_muted" = 0 ;;
    *) exit 1 ;;
esac
audio_left=
audio_right=
audio_muted=
if "$directory/companionctl" --socket "$socket" sound-settings '{"volume_left":120,"volume_right":10}' >/tmp/companion-native-dell-sound-rejected.json; then exit 1; fi
result=$("$directory/companionctl" --socket "$socket" network-scan)
case "$result" in *'"ok":true'*'"interface":"wlan0"'*'"networks":['*) ;; *) echo "Wireless scan failed: $result" >&2; exit 1 ;; esac
result=$("$directory/companionctl" --socket "$socket" network-status)
case "$result" in *'"ok":true'*'"interface":"wlan0"'*'"state":"'*'"address":"'*) ;; *) echo "Wireless status failed: $result" >&2; exit 1 ;; esac
if "$directory/companionctl" --socket "$socket" network-connect '{"ssid":"bad\nname","password":"test-password"}' >/tmp/companion-native-dell-wifi-rejected.json; then exit 1; fi
if "$directory/companionctl" --socket "$socket" network-connect '{"ssid":"test","password":"short"}' >/tmp/companion-native-dell-wifi-rejected.json; then exit 1; fi
if "$directory/companionctl" --socket "$socket" bios-set '{"name":"BootOrder","value":"bad"}' >/tmp/companion-native-dell-bios-rejected.json; then exit 1; fi
if "$directory/companionctl" --socket "$socket" power '{"operation":"reboot","confirm":false}' >/tmp/companion-native-dell-rejected.json; then exit 1; fi
echo 'Dell native control sidecar checks passed'

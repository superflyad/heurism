#!/bin/sh
# Deliberately unassociable SSID: prove C rollback without changing Ethernet.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
test ! -e /etc/companion/wifi.conf
test ! -e /run/companion/wpa-wlan0.pid
stage=/var/lib/companion/native-stage
directory=$(mktemp -d /tmp/heurism-native-wifi.XXXXXX)
chmod 755 "$directory"
install -m 755 "$stage/heurism-control" "$stage/heurismctl" "$directory/"
socket=/tmp/heurism-control-native-wifi-$$.sock
state=/tmp/heurism-native-wifi-preferences-$$.json
before=$(ip -4 -o addr show dev eth0)
"$directory/heurism-control" --socket "$socket" --state "$state" \
    >"$directory/control.log" 2>&1 &
server=$!
cleanup() {
    kill "$server" 2>/dev/null || true
    wait "$server" 2>/dev/null || true
    rm -f "$state" "$socket"
    rm -f "$directory/heurism-control" "$directory/heurismctl" \
        "$directory/control.log" "$directory/result.json"
    rmdir "$directory"
}
trap cleanup EXIT HUP INT TERM
attempt=0
while [ ! -S "$socket" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 20
    sleep 1
done
name=HeurismNoAP-$$
if "$directory/heurismctl" --socket "$socket" network-connect \
    "{\"ssid\":\"$name\",\"password\":\"test-only-passphrase\"}" \
    >"$directory/result.json"; then
    echo 'Unexpected association with nonexistent test network' >&2
    exit 1
fi
case "$(cat "$directory/result.json")" in
    *'"ok":false'*'prior settings restored'*) ;;
    *) cat "$directory/result.json" >&2; exit 1 ;;
esac
test ! -e /etc/companion/wifi.conf
test ! -e /run/companion/wpa-wlan0.pid
test "$(ip -4 -o addr show dev eth0)" = "$before"
rc-service sshd status >/dev/null
rc-service companion-watch status >/dev/null
sha256sum -c /var/lib/companion/presentation-20260927T161814Z/protected.sha256 >/dev/null
rm -f "$directory/result.json"
echo 'Dell native Wi-Fi failed-association rollback passed; Ethernet unchanged'

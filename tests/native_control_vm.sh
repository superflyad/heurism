#!/bin/sh
# Exercise the C control service beside the active Python service on CompanionDev.
set -eu
stage=${1:-/var/lib/companion/native-stage}
socket=/tmp/heurism-control-native-test-$$.sock
state=/tmp/heurism-native-preferences-$$.json
rm -f "$state" "$state.new"
"$stage/heurism-control" --socket "$socket" --state "$state" \
    >/tmp/heurism-native-control.log 2>&1 &
server=$!
trap 'kill "$server" 2>/dev/null || true; wait "$server" 2>/dev/null || true; rm -f "$state" "$socket" /tmp/heurism-native-admin-rejected.json /tmp/heurism-native-input-rejected.json' EXIT HUP INT TERM
attempt=0
while [ ! -S "$socket" ]; do
    attempt=$((attempt + 1))
    test "$attempt" -lt 30
    sleep 1
done
result=$("$stage/heurismctl" --socket "$socket" status)
case "$result" in
    *'"ok":true'*'"platform":"hyperv-dev"'*'"management"'*) ;;
    *) echo 'native status response invalid' >&2; exit 1 ;;
esac
result=$(su -s /bin/sh -c "$stage/heurismctl --socket $socket status" companion-ui)
case "$result" in *'"ok":true'*) ;; *) exit 1 ;; esac
if su -s /bin/sh -c "$stage/heurismctl --socket $socket admin-console" companion-ui \
    >/tmp/heurism-native-admin-rejected.json; then exit 1; fi
case "$(cat /tmp/heurism-native-admin-rejected.json)" in
    *'Use authenticated root SSH for administration'*) ;;
    *) echo 'Desktop caller reached root console' >&2; exit 1 ;;
esac
result=$("$stage/heurismctl" --socket "$socket" theme '"light"')
case "$result" in *'"theme":"light"'*) ;; *) exit 1 ;; esac
case "$(cat "$state")" in *'"theme":"light"'*) ;; *) exit 1 ;; esac
if "$stage/heurismctl" --socket "$socket" input-status >/tmp/heurism-native-input-rejected.json; then exit 1; fi
case "$(cat /tmp/heurism-native-input-rejected.json)" in
    *'The libinput touchpad is unavailable'*) ;;
    *) exit 1 ;;
esac
result=$("$stage/heurismctl" --socket "$socket" power-check)
case "$result" in *'"ok":true'*'"data":true'*) ;; *) exit 1 ;; esac
if "$stage/heurismctl" --socket "$socket" power '{"operation":"reboot","confirm":false}' >/tmp/heurism-native-power-rejected.json; then exit 1; fi
case "$(cat /tmp/heurism-native-power-rejected.json)" in *'Confirm the specific power action'*) ;; *) exit 1 ;; esac
if "$stage/heurismctl" --socket "$socket" network-scan >/tmp/heurism-native-error.json; then exit 1; fi
case "$(cat /tmp/heurism-native-error.json)" in *'Dell wireless interface unavailable'*) ;; *) exit 1 ;; esac
echo 'native control socket checks passed'

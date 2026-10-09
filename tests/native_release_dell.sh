#!/bin/sh
# Verify sealed release rejection without altering the active Dell desktop.
set -eu
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
candidate=$(cat /var/lib/companion/native-dell-candidate)
"$candidate/heurism-release" verify "$candidate" >/dev/null
if [ -L /opt/heurism/native/current ]; then
    before=$(readlink -f /opt/heurism/native/current)
else
    before=
fi
clone=$(mktemp -d /opt/heurism/native/releases/c-dell-broken.XXXXXX)
cleanup() {
    resolved=$(readlink -f "$clone")
    case "$resolved" in /opt/heurism/native/releases/c-dell-broken.*) rm -rf "$resolved" ;;
        *) echo 'Refusing unexpected cleanup path' >&2; exit 1 ;;
    esac
}
trap cleanup EXIT HUP INT TERM
cp -a "$candidate/." "$clone/"
printf 'broken\n' >>"$clone/heurism-desktop"
if "$candidate/heurism-release" verify "$clone" >/dev/null 2>&1; then
    echo 'Altered release passed verification' >&2
    exit 1
fi
if [ -n "$before" ]; then
    test "$(readlink -f /opt/heurism/native/current)" = "$before"
else
    test ! -e /opt/heurism/native/current
fi
rc-service companion-control status >/dev/null
rc-service companion-desktop status >/dev/null
rc-service companion-watch status >/dev/null
rc-service sshd status >/dev/null
echo 'Dell sealed-release corruption rejection passed; active desktop unchanged'

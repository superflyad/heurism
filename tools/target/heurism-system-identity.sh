#!/bin/sh
# Install Heurism OS identity while preserving the packaged Alpine provenance.
set -eu
action=${1:-}
profile=${2:-}
case "$action" in check|install|verify|rollback) ;; *) echo 'usage: heurism-system-identity.sh check|install|verify|rollback vm|dell' >&2; exit 2 ;; esac
case "$profile" in
vm)
    manifest=/etc/companion/vm-protected.sha256
    test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Microsoft Corporation'
    test "$(cat /etc/companion/platform.json)" = '{"platform":"hyperv-dev"}' ;;
dell)
    manifest=/var/lib/companion/presentation-20260927T161814Z/protected.sha256
    test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
    test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
    test ! -e /etc/companion/platform.json ;;
*) echo 'Unsupported platform profile' >&2; exit 2 ;;
esac
stage=/var/lib/heurism/system-identity
identity_hash=4cb2272e2f85fe61c37fab906149fb8624035ef38847d5b5439ba8e1d7bbb367
upstream_hash=1eb5561b4eae9962ff0f16ba7900cdc1f444535490f4863954e5bceab26beab3
hash_file() { sha256sum "$1" | cut -d ' ' -f 1; }
base_check() {
    test "$(id -u)" = 0
    test -d "$stage" && test ! -L "$stage"
    test "$(stat -c %u "$stage")" = 0
    test "$(stat -c %a "$stage")" = 700
    test "$(hash_file "$stage/os-release")" = "$identity_hash"
    test "$(hash_file "$stage/upstream-release")" = "$upstream_hash"
    grep -Fqx ID=alpine /usr/lib/os-release
    grep -Fqx VERSION_ID=3.24.2 /usr/lib/os-release
    apk info -e alpine-base >/dev/null
    rc-service sshd status >/dev/null
    rc-service companion-watch status >/dev/null
    test "$(cat /proc/sys/kernel/random/boot_id)" = "$(cat /var/lib/companion/healthy-boot-id)"
    /opt/heurism/native/current/heurism-release verify >/dev/null
    /opt/heurism/native/current/heurism-release health >/dev/null
    sha256sum -c "$manifest" >/dev/null
    heurismctl power-check | grep -Fq '"ok":true'
}
old_check() {
    base_check
    test -L /etc/os-release
    test "$(readlink /etc/os-release)" = ../usr/lib/os-release
    test ! -e /etc/heurism/upstream-release
    test ! -e "$stage/os-release.before"
    test ! -e "$stage/manifest.before"
    ! grep -Fq '  /etc/os-release' "$manifest"
    ! grep -Fq '  /etc/heurism/upstream-release' "$manifest"
}
new_check() {
    base_check
    test -f /etc/os-release && test ! -L /etc/os-release
    test "$(hash_file /etc/os-release)" = "$identity_hash"
    test "$(hash_file /etc/heurism/upstream-release)" = "$upstream_hash"
    test -L "$stage/os-release.before"
    test "$(readlink "$stage/os-release.before")" = ../usr/lib/os-release
    sha256sum -c "$stage/vendor-before.sha256" >/dev/null
    grep -Fqx "$identity_hash  /etc/os-release" "$manifest"
    grep -Fqx "$upstream_hash  /etc/heurism/upstream-release" "$manifest"
}
restore() {
    trap - EXIT HUP INT TERM
    if [ -L "$stage/os-release.before" ]; then
        rm -f /etc/os-release
        mv "$stage/os-release.before" /etc/os-release
    fi
    rm -f /etc/heurism/upstream-release
    if [ -f "$stage/manifest.before" ]; then
        cp -p "$stage/manifest.before" "$manifest.rollback"
        mv -f "$manifest.rollback" "$manifest"
    fi
    echo 'Heurism OS identity activation failed; previous identity restored' >&2
}
exec 9>/run/heurism-system-identity.lock
flock -n 9
case "$action" in
check)
    old_check
    echo 'Heurism identity preflight passed' ;;
install)
    old_check
    cp -p "$manifest" "$stage/manifest.before"
    sha256sum /usr/lib/os-release >"$stage/vendor-before.sha256"
    trap restore EXIT HUP INT TERM
    mv /etc/os-release "$stage/os-release.before"
    install -d -m 755 /etc/heurism
    install -m 644 "$stage/os-release" /etc/os-release.next
    install -m 644 "$stage/upstream-release" /etc/heurism/upstream-release.next
    mv -f /etc/os-release.next /etc/os-release
    mv -f /etc/heurism/upstream-release.next /etc/heurism/upstream-release
    cp "$stage/manifest.before" "$stage/manifest.next.sha256"
    sha256sum /etc/os-release /etc/heurism/upstream-release >>"$stage/manifest.next.sha256"
    install -m 644 "$stage/manifest.next.sha256" "$manifest.next"
    mv -f "$manifest.next" "$manifest"
    new_check
    trap - EXIT HUP INT TERM
    echo 'Heurism OS identity active; packaged Alpine source retained' ;;
verify)
    new_check
    echo 'Heurism OS identity and protected manifest verified' ;;
rollback)
    new_check
    restore
    sha256sum -c "$manifest" >/dev/null
    test -L /etc/os-release
    echo 'Previous OS identity restored' ;;
esac

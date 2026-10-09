#!/bin/sh
# Create only the second, pinned owner variable. Never replace or delete either.
set -eu
action=${1:-}
case "$action" in check|create|verify) ;; *) echo 'usage: heurism-nv-stage.sh check|create|verify' >&2; exit 2 ;; esac
guid=1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1
old=/sys/firmware/efi/efivars/CompanionExtensionImage01-$guid
new=/sys/firmware/efi/efivars/HeurismExtensionImage01-$guid
stage=/var/lib/companion/firmware/heurism-nv-candidate
driver=/boot/efi/EFI/companion/companionextx64.efi
manifest=/var/lib/companion/presentation-20260927T161814Z/protected.sha256
payload_hash=b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a
old_driver_hash=2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4
candidate_hash=a215f4143742e4263577942e1886b5546375bf08777f11903196e0f99c78ff31
hash_file() { sha256sum "$1" | cut -d ' ' -f 1; }
check_var() {
    test -f "$1"
    test "$(stat -c %s "$1")" = 2052
    test "$(od -An -tu4 -N4 "$1" | tr -d ' ')" = 7
    test "$(tail -c +5 "$1" | sha256sum | cut -d ' ' -f 1)" = "$payload_hash"
}
test "$(id -u)" = 0
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X
test "$(cat /proc/sys/kernel/random/boot_id)" = "$(cat /var/lib/companion/healthy-boot-id)"
test "$(efibootmgr | sed -n 's/^BootCurrent: //p')" = 0005
test "$(efibootmgr | sed -n 's/^BootOrder: //p')" = 0005,0000
test "$(efibootmgr --driver | sed -n 's/^DriverOrder: //p')" = 0000,0001
! efibootmgr | grep -q '^BootNext:'
test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1
test "$(findmnt -n -o FSTYPE /sys/firmware/efi/efivars)" = efivarfs
rc-service sshd status >/dev/null
rc-service companion-watch status >/dev/null
rc-service heurism-control status >/dev/null
sha256sum -c "$manifest" >/dev/null
test "$(hash_file "$driver")" = "$old_driver_hash"
test "$(hash_file "$stage/dual-name.efi")" = "$candidate_hash"
check_var "$old"
if [ "$action" = create ]; then
    test ! -e "$new"
    test ! -L "$stage"
    test "$(stat -c %u "$stage")" = 0
    test "$(stat -c %a "$stage")" = 700
    raw=$stage/owner-variable.raw
    test ! -e "$raw"
    cp "$old" "$raw"
    chmod 600 "$raw"
    check_var "$raw"
    cmp "$old" "$raw"
    # efivarfs expects attributes plus data in one write. Input is a 2052-byte
    # regular file; one dd block performs one write to the new variable.
    dd if="$raw" of="$new" bs=2052 count=1 2>"$stage/variable-write.log"
    sync
fi
if [ "$action" = verify ] || [ "$action" = create ]; then
    check_var "$new"
    cmp "$old" "$new"
fi
printf 'legacy owner intact; active EFI driver unchanged; action=%s\n' "$action"

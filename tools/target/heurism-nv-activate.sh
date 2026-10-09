#!/bin/sh
# Dell-only SSD bootstrap switch. No boot entry or NVRAM variable writes.
set -eu
action=${1:-}
case "$action" in check|activate|verify|rollback) ;; *) echo 'usage: heurism-nv-activate.sh check|activate|verify|rollback' >&2; exit 2 ;; esac
stage=/var/lib/companion/firmware/heurism-nv-candidate
driver=/boot/efi/EFI/companion/companionextx64.efi
esp_backup=/boot/efi/EFI/companion/companionextx64.before-heurism.efi
manifest=/var/lib/companion/presentation-20260927T161814Z/protected.sha256
old_hash=2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4
new_hash=a215f4143742e4263577942e1886b5546375bf08777f11903196e0f99c78ff31
guid=1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1
legacy_var=/sys/firmware/efi/efivars/CompanionExtensionImage01-$guid
heurism_var=/sys/firmware/efi/efivars/HeurismExtensionImage01-$guid
hash_file() { sha256sum "$1" | cut -d ' ' -f 1; }
identity_check() {
    test "$(id -u)" = 0
    test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
    test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
    test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X
    test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1
    test ! -L "$stage"
    test "$(stat -c %u "$stage")" = 0
    test "$(stat -c %a "$stage")" = 700
}
base_check() {
    identity_check
    test "$(cat /proc/sys/kernel/random/boot_id)" = "$(cat /var/lib/companion/healthy-boot-id)"
    test "$(efibootmgr | sed -n 's/^BootCurrent: //p')" = 0005
    test "$(efibootmgr | sed -n 's/^BootOrder: //p')" = 0005,0000
    test "$(efibootmgr --driver | sed -n 's/^DriverOrder: //p')" = 0000,0001
    ! efibootmgr | grep -q '^BootNext:'
    test "$(hash_file "$stage/dual-name.efi")" = "$new_hash"
    rc-service sshd status >/dev/null
    rc-service companion-watch status >/dev/null
    rc-service heurism-control status >/dev/null
    rc-service heurism-desktop status >/dev/null
    sha256sum -c "$manifest" >/dev/null
    test "$(df -Pk /boot/efi | awk 'NR==2 {print $4}')" -gt 1024
}
old_check() {
    base_check
    test "$(hash_file "$driver")" = "$old_hash"
    test ! -e "$stage/driver-before.efi"
    test ! -e "$stage/manifest-before.sha256"
    test ! -e "$esp_backup"
    sh "$stage/stage-variable.sh" verify >/dev/null
    grep -Fqx "$old_hash  $driver" "$manifest"
    heurismctl power-check | grep -Fq '"ok":true'
}
restore() {
    trap - EXIT HUP INT TERM
    if [ -f "$stage/driver-before.efi" ] &&
       [ -f "$stage/manifest-before.sha256" ] &&
       [ "$(hash_file "$stage/driver-before.efi")" = "$old_hash" ]; then
        cp "$stage/driver-before.efi" "$driver.rollback"
        sync
        mv -f "$driver.rollback" "$driver"
        cp -p "$stage/manifest-before.sha256" "$manifest.rollback"
        mv -f "$manifest.rollback" "$manifest"
        sync
        sha256sum -c "$manifest" >/dev/null || true
    fi
    echo 'EFI activation failed; attempted to restore prior driver and manifest' >&2
}
exec 9>/run/heurism-nv-activate.lock
flock -n 9
case "$action" in
check)
    old_check
    echo 'dual-name candidate and physical activation preflight passed' ;;
activate)
    old_check
    cp -p "$driver" "$stage/driver-before.efi"
    cp -p "$manifest" "$stage/manifest-before.sha256"
    cp "$driver" "$esp_backup"
    sync
    test "$(hash_file "$stage/driver-before.efi")" = "$old_hash"
    test "$(hash_file "$esp_backup")" = "$old_hash"
    cmp "$manifest" "$stage/manifest-before.sha256"
    trap restore EXIT HUP INT TERM
    awk -v path="$driver" -v old="$old_hash" -v new="$new_hash" '
      $2==path {if ($1!=old) exit 2; print new "  " path; found++; next}
      {print}
      END {if (found!=1) exit 3}
    ' "$manifest" >"$stage/manifest-next.sha256"
    install -m 644 "$stage/manifest-next.sha256" "$manifest.next"
    cp "$stage/dual-name.efi" "$driver.next"
    sync
    test "$(hash_file "$driver.next")" = "$new_hash"
    mv -f "$driver.next" "$driver"
    mv -f "$manifest.next" "$manifest"
    sync
    test "$(hash_file "$driver")" = "$new_hash"
    sha256sum -c "$manifest" >/dev/null
    heurismctl power-check | grep -Fq '"ok":true'
    trap - EXIT HUP INT TERM
    echo 'dual-name SSD driver active; reboot and path-3 marker verification pending' ;;
verify)
    base_check
    test "$(hash_file "$driver")" = "$new_hash"
    test "$(hash_file "$stage/driver-before.efi")" = "$old_hash"
    test "$(hash_file "$esp_backup")" = "$old_hash"
    test "$(stat -c %s "$legacy_var")" = 2052
    test "$(stat -c %s "$heurism_var")" = 2052
    test "$(od -An -tu4 -N4 "$heurism_var" | tr -d ' ')" = 7
    cmp "$legacy_var" "$heurism_var"
    grep -Fqx "$new_hash  $driver" "$manifest"
    heurismctl power-check | grep -Fq '"ok":true'
    echo 'dual-name SSD driver and protected manifest verified' ;;
rollback)
    identity_check
    test "$(hash_file "$driver")" = "$new_hash"
    test "$(hash_file "$stage/driver-before.efi")" = "$old_hash"
    test "$(hash_file "$esp_backup")" = "$old_hash"
    grep -Fqx "$new_hash  $driver" "$manifest"
    cp "$stage/driver-before.efi" "$driver.rollback"
    cp -p "$stage/manifest-before.sha256" "$manifest.rollback"
    sync
    mv -f "$driver.rollback" "$driver"
    mv -f "$manifest.rollback" "$manifest"
    sync
    test "$(hash_file "$driver")" = "$old_hash"
    sha256sum -c "$manifest" >/dev/null
    echo 'prior SSD driver and manifest restored; Heurism variable retained' ;;
esac

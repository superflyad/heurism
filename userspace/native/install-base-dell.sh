#!/bin/sh
# Install only the C shell and terminal; leave Dell services and boot untouched.
set -eu
stage=/var/lib/companion/native-stage
root=/opt/companion/native
test "$(id -u)" = 0
test "$(cat /sys/class/dmi/id/sys_vendor)" = 'Dell Inc.'
test "$(cat /sys/class/dmi/id/product_name)" = 'Inspiron 7506 2n1'
test ! -e /etc/companion/platform.json
test ! -e "$root/current"
test ! -e /usr/local/bin/companion-sh
test ! -e /usr/local/bin/companion-terminal
sh "$stage/native_shell.sh" "$stage/companion-sh"
name=c-base-dell-$(date -u +%Y%m%dT%H%M%SZ)-$$
release=$root/releases/$name
install -d -m 755 "$release"
install -m 755 "$stage/companion-sh" "$release/companion-sh"
install -m 755 "$stage/companion-terminal" "$release/companion-terminal"
(
    cd "$release"
    sha256sum companion-sh companion-terminal > hashes.sha256
    sha256sum -c hashes.sha256
)
test "$(su -s /bin/sh -c "$release/companion-sh -c 'id -u'" companion-ui)" = 1000
ln -s "$release" "$root/current.next"
mv -T "$root/current.next" "$root/current"
ln -s "$root/current/companion-sh" /usr/local/bin/companion-sh
ln -s "$root/current/companion-terminal" /usr/local/bin/companion-terminal
printf 'Installed native shell/terminal release %s; desktop/control services unchanged\n' "$release"

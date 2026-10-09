#!/bin/sh
# Install the tested C executables without changing the active desktop services.
set -eu
stage=${1:-/var/lib/companion/native-stage}
name=c-base-0.4
release=/opt/companion/native/releases/$name
test -x "$stage/companion-sh"
test -x "$stage/companion-terminal"
test ! -e /opt/companion/native/current.next
test ! -L /opt/companion/native/current.next
for program in companion-sh companion-terminal; do
    link=/usr/local/bin/$program
    if [ -e "$link" ] || [ -L "$link" ]; then
        test "$(readlink "$link")" = "/opt/companion/native/current/$program"
    fi
done
sh "$stage/native_shell.sh" "$stage/companion-sh"
install -d -m 755 "$release"
install -m 755 "$stage/companion-sh" "$stage/companion-terminal" "$release/"
ln -s "releases/$name" /opt/companion/native/current.next
mv -fT /opt/companion/native/current.next /opt/companion/native/current
for program in companion-sh companion-terminal; do
    link=/usr/local/bin/$program
    if [ ! -L "$link" ]; then
        ln -s "/opt/companion/native/current/$program" "$link"
    fi
done
su -s /bin/sh -c '/opt/companion/native/current/companion-sh -c "id -u"' companion-ui

#!/bin/sh
# Install the tested C executables without changing the active desktop services.
set -eu
stage=${1:-/var/lib/companion/native-stage}
name=c-base-0.4
release=/opt/heurism/native/releases/$name
test -x "$stage/heurism-sh"
test -x "$stage/heurism-terminal"
test ! -e /opt/heurism/native/current.next
test ! -L /opt/heurism/native/current.next
for program in heurism-sh heurism-terminal; do
    link=/usr/local/bin/$program
    if [ -e "$link" ] || [ -L "$link" ]; then
        test "$(readlink "$link")" = "/opt/heurism/native/current/$program"
    fi
done
sh "$stage/native_shell.sh" "$stage/heurism-sh"
install -d -m 755 "$release"
install -m 755 "$stage/heurism-sh" "$stage/heurism-terminal" "$release/"
ln -s "releases/$name" /opt/heurism/native/current.next
mv -fT /opt/heurism/native/current.next /opt/heurism/native/current
for program in heurism-sh heurism-terminal; do
    link=/usr/local/bin/$program
    if [ ! -L "$link" ]; then
        ln -s "/opt/heurism/native/current/$program" "$link"
    fi
done
su -s /bin/sh -c '/opt/heurism/native/current/heurism-sh -c "id -u"' companion-ui

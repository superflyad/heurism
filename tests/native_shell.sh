#!/bin/sh
# Behavioral checks for the C shell. Uses only POSIX tools and a private directory.
set -eu
shell=${1:?pass heurism-sh binary}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

test "$("$shell" --version)" = 'Heurism shell 0.1 (C/POSIX)'
test "$("$shell" -c 'printf "<%s>" "a b"')" = '<a b>'
test "$("$shell" -c "printf '<%s>' ''")" = '<>'
test "$("$shell" -c 'printf "abc" | tr a-z A-Z')" = ABC
test "$("$shell" -c 'cd /tmp; pwd')" = /tmp
test "$("$shell" -c 'export HEURISM_TEST=ready; printf "%s" "$HEURISM_TEST"')" = ready
test "$("$shell" -c 'false; printf "%s" "$?"')" = 1
test "$("$shell" -c 'printf "%s" "a;b"; printf z')" = 'a;bz'
test "$("$shell" -c 'printf ok # ignore; printf bad')" = ok
test "$("$shell" -c 'printf "#"; printf z')" = '#z'
test "$("$shell" -c 'printf "%s" "$HOME"')" = "$HOME"
"$shell" -c "printf one > $tmp/output; printf two >> $tmp/output"
test "$(cat "$tmp/output")" = onetwo
"$shell" -c "ls $tmp/missing 2> $tmp/error" && exit 1
test -s "$tmp/error"
set +e
"$shell" -c 'exit 7' >/dev/null
status=$?
set -e
test "$status" = 7
"$shell" -c 'printf x |' >/dev/null 2>&1 && exit 1
"$shell" -c 'export 9BAD=x' >/dev/null 2>&1 && exit 1
printf x >"$tmp/part-a"
printf y >"$tmp/part-b"
test "$("$shell" -c "printf '<%s>' $tmp/part-*")" = "<$tmp/part-a><$tmp/part-b>"
test "$("$shell" -c "printf '<%s>' '$tmp/part-*'")" = "<$tmp/part-*>"
test "$("$shell" -c "printf '<%s>' $tmp/missing-*")" = "<$tmp/missing-*>"
echo 'native shell checks passed'

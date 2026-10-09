#!/bin/sh
# Exercise line editing over a real PTY, including history and cursor insertion.
set -eu
shell=${1:?pass companion-sh binary}
command -v script >/dev/null
directory=$(mktemp -d /tmp/companion-shell-interactive.XXXXXX)
trap 'rm -rf "$directory"' EXIT HUP INT TERM

printf 'printf x >> %s\r\033[A\r\004' "$directory/history" |
    timeout 15 script -q -e -c "$shell" "$directory/history.log" >/dev/null
test "$(cat "$directory/history")" = xx

printf 'printf y > %s\033[Dl\r\004' "$directory/fie" |
    timeout 15 script -q -e -c "$shell" "$directory/edit.log" >/dev/null
test "$(cat "$directory/file")" = y

echo 'native interactive shell history and editing checks passed'

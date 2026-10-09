#!/bin/sh
# Exercise line editing over a real PTY, including history and cursor insertion.
set -eu
shell=${1:?pass heurism-sh binary}
command -v script >/dev/null
directory=$(mktemp -d /tmp/heurism-shell-interactive.XXXXXX)
trap 'rm -rf "$directory"' EXIT HUP INT TERM

printf 'printf x >> %s\r\033[A\r\004' "$directory/history" |
    timeout 15 script -q -e -c "$shell" "$directory/history.log" >/dev/null
test "$(cat "$directory/history")" = xx

printf 'printf y > %s\033[Dl\r\004' "$directory/fie" |
    timeout 15 script -q -e -c "$shell" "$directory/edit.log" >/dev/null
test "$(cat "$directory/file")" = y

{
    awk 'BEGIN {for (i = 0; i < 8192; i++) printf "A"}'
    printf '\rprintf recovered > %s\r\004' "$directory/after-overflow"
} | timeout 15 script -q -e -c "$shell" "$directory/overflow.log" >/dev/null
test "$(cat "$directory/after-overflow")" = recovered
grep -q 'line exceeds 8192 bytes' "$directory/overflow.log"

echo 'native interactive shell history and editing checks passed'

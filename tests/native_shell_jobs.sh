#!/bin/sh
# Verify foreground terminal ownership, stop/resume and background completion.
set -eu
shell=${1:?pass heurism-sh binary}
command -v script >/dev/null
directory=$(mktemp -d /tmp/heurism-shell-jobs.XXXXXX)
trap 'rm -rf "$directory"' EXIT HUP INT TERM
{
    printf 'sleep 20\r'
    sleep 1
    printf '\032'
    sleep 1
    printf 'jobs\r'
    sleep 1
    printf 'bg %%1\r'
    sleep 1
    printf 'fg %%1\r'
    sleep 1
    printf '\003'
    sleep 1
    printf 'sleep 20 | cat\r'
    sleep 1
    printf '\003'
    sleep 1
    printf 'sleep 1 | cat &\r'
    sleep 1
    printf 'jobs\r'
    sleep 2
    printf 'jobs\r'
    sleep 1
    printf 'cat > %s\r' "$directory/foreground-input"
    sleep 1
    printf 'terminal ownership works\r'
    sleep 1
    printf '\003'
    sleep 1
    printf 'exit 0\r'
} | timeout 30 script -q -e -c "$shell" "$directory/session.log" >/dev/null
grep -q 'Stopped sleep 20' "$directory/session.log"
grep -q 'Running sleep 20' "$directory/session.log"
grep -q 'Done sleep 1 | cat' "$directory/session.log"
grep -q '^terminal ownership works' "$directory/foreground-input"
! grep -q 'tcsetpgrp:' "$directory/session.log"
test "$("$shell" -c 'true & printf done')" = done
if "$shell" -c 'jobs &' >"$directory/invalid.log" 2>&1; then
    echo 'job control builtin ran in background' >&2
    exit 1
fi
grep -q 'requires the foreground shell' "$directory/invalid.log"
echo 'native shell job-control PTY checks passed'

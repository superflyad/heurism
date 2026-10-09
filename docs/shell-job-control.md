# Heurism shell job control in the development VM

The C shell now starts every external command or pipeline in its own process
group. For a foreground job it transfers the controlling PTY to that group,
waits for exit or stop, then takes the PTY back. A launch gate keeps a child
from reading the PTY before the transfer. The shell itself ignores terminal
stop signals while it owns the prompt. Its children restore default signal
handling.

An unquoted `&` starts a background job. `jobs` lists live jobs; `fg` and `bg`
accept `%N` or a plain job number and otherwise select the most recently
started live job. Ctrl+Z stops a foreground job, and Ctrl+C interrupts it.
When the shell exits or receives SIGHUP from a closing terminal, it sends
SIGHUP and SIGCONT to all tracked job groups. Job control builtins are rejected
inside pipelines or in the background.

## VM evidence, 2026-10-09

On `CompanionDev` boot `0b1b0863-01f4-4406-a6bd-a9a2c37e4cc7`, the C source
compiled with strict warnings and stack protection. A real `script` PTY test
stopped `sleep 20` with Ctrl+Z, listed it, resumed it with `bg %1`, brought it
back with `fg %1`, and interrupted it with Ctrl+C. The same test interrupted a
foreground pipeline, completed a background pipeline, and proved an input
reading `cat` owned the terminal. Existing shell and interactive editing tests
passed. The new job test is an `assemble` gate in the VM installer.

The active sealed VM release is
`/opt/heurism/native/releases/heurism-os-20261009T175459Z-69087`.
Its shell SHA256 is
`48f7636175eec75e58ad263951c40698ef45666a960431de2f0e4182f297a0b6`.
The release verifier, Xfce health, C terminal PTY test, SSH and watch passed.
In the actual X11 terminal, a background `sleep 100` exited when its window
closed; no terminal process remained. Temporary build packages were removed.
The preceding release is retained for the VM installer's rollback path. No VM
reboot or cold-boot check was run for this release. No Dell command was run.

## Limits

This is interactive job control, not a complete POSIX shell. Job completion is
reported when the shell next processes input; it does not asynchronously
redraw the prompt. There is no `disown`, `wait`, job notification preference,
completion, or persistent session. Background jobs end with the shell. The
shell's quoting, expansion and script language remain smaller than BusyBox
ash, which continues to run startup and recovery scripts. The Dell has not
received this release.

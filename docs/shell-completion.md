# Heurism shell completion

The C shell now completes an unquoted word at the end of an interactive line
when Tab is pressed. The first word searches executable names in `PATH` and
the shell builtins. An argument searches the corresponding directory. A
unique directory gets `/`; a unique file or command gets a following space.
Multiple matches extend only their common prefix. The shell escapes spaces
and metacharacters added from a filename. No candidate is executed during
lookup. Quoted, escaped, wildcard and variable-containing partial words, and
completion in the middle of a line, are deliberately left unchanged. The
shell rings the terminal bell if it cannot extend the line. Startup scripts
continue to use BusyBox ash.

Source revision `0572384` added the implementation and real-PTY regression
cases for a filename with a space, a directory and an executable in `PATH`.
The strict C build, existing shell tests, interactive line editing and
job-control PTY tests all passed in `CompanionDev`. The guarded VM release is
`/opt/heurism/native/releases/heurism-os-20261010T005501Z-24116`.
An actual UID-1000 C terminal completed `notes_` into `notes_draft.txt` and
copied the file through its PTY. Checked C reboot returned fresh VM boot
`111a682d-c551-4eb6-879d-b1791b161726` with that release, SSH/watch/control
and painted Xfce healthy. VM checkpoint before this work:
`Heurism-shell-completion-pre-20261009`, UUID
`1674cf62-28f7-40b6-a09d-826417e730e1`. Temporary build packages were
removed.

The Dell-only installer then passed the same shell and job-control PTY tests,
its desktop/app/control sidecars, release verification and corrupt-clone
rejection. It activated sealed release
`/opt/heurism/native/releases/heurism-os-dell-20261010T010017Z-7933` on
existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41` without an OS reboot.
The installed shell SHA256 is
`81babcf7c1cbed058d22198a52a770f13aae03e3134c72155f4482cdaa202d77`.
A real Dell UID-1000 terminal completed the same filename and wrote a checked
file. SSH/watch/control/UI health, checked power and protected SSD/NVRAM
verification passed. Rollback files are at
`/var/lib/companion/native-dell-init-backup-20261010T010025Z-12087`.
The old release remains on disk. This release has not had a Dell OS reboot;
the firmware-logo recovery guard remains active.

This is a bounded usability step, not a full POSIX shell. Quoted completion,
listing alternatives, variable expansion in paths and script-shell semantics
remain future work. See [the OS checklist](os-checklist.md) and
[the terminal record](terminal-clipboard.md).

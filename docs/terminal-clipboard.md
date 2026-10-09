# Heurism terminal clipboard, VM verification

The C terminal owns its X11 window, clipboard selections and PTY. libvterm
still parses display control sequences, and `heurism-sh` remains the command
interpreter. These changes are active only in `CompanionDev`. The Dell later
returned on a healthy fresh boot with its preceding scrollback release; these
clipboard changes have not been installed there. No Dell command was run for
the VM clipboard work.

## Behavior

- Drag over text to select it and own X11 PRIMARY. Middle click or
  Shift+Insert pastes PRIMARY. Ctrl+Shift+C copies the selection to CLIPBOARD;
  Ctrl+Shift+V pastes CLIPBOARD. UTF8_STRING exchange preserves non-ASCII bytes.
- Paste is capped at 8192 bytes. NUL, Escape, DEL and other control bytes are
  rejected; tab and line endings are allowed. Oversized or rejected paste
  leaves the terminal process running and shows a short title notice.
- A paste containing a line ending is held for ten seconds. Repeating the same
  paste action confirms that captured text. The first action sends no bytes to
  the PTY. After confirmation, line endings may execute shell commands, as
  normal terminal input would. The terminal calls libvterm's paste hooks;
  bracketed-paste exchange with a requesting application still needs an
  end-to-end test.
- Scrollback position remains visible in the window title. Closing a terminal
  window now resets an inherited ignored SIGHUP in the child and uses bounded
  HUP, TERM and KILL cleanup if the child does not exit. It handles a destroyed
  X11 window without a stale Render picture request.
- The C shell rejects an overlong interactive line without draining the next
  already-entered command. Script and recovery shell behavior remains with
  BusyBox ash.

## Evidence on 2026-10-09

`CompanionDev` assembled and activated sealed release
`/opt/heurism/native/releases/heurism-os-20261009T172555Z-50199` on boot
`0b1b0863-01f4-4406-a6bd-a9a2c37e4cc7`. The release hashes and live Xfce
health verified; SSH and watch stayed running. The installer sidecars passed
Xfce/apps, shell, interactive shell and control checks. The live UID-1000
terminal then passed PTY input/output, pipeline, resize and Ctrl+C checks.

An independent `xclip` process supplied CLIPBOARD text; Ctrl+Shift+V entered
the command through the real PTY. Drag selection served text to `xclip` through
PRIMARY, Ctrl+Shift+C served it through CLIPBOARD, and middle click pasted
PRIMARY from `xclip`. The selection returned the exact UTF-8 bytes of
`héllo 世界` even though the installed font rendered the CJK characters as
missing glyph boxes. A 9000-byte paste and an Escape-containing paste were
rejected without entering the shell; subsequent commands worked. The first
multiline paste changed the title to request confirmation and created no
files. The second paste sent the text, and the harmless test commands ran.
Closing a candidate and then the active-release terminal ended their processes
without a surviving shell or X11 error.

A second VM-only release,
`/opt/heurism/native/releases/heurism-os-20261009T173229Z-54707`, adds a
WenQuanYi Zen Hei Mono fallback when DejaVu Sans Mono lacks a glyph. The VM
installed `font-wqy-zenhei` (about 16 MiB installed), and a capture of the
active release shows `héllo 世界` with visible CJK glyphs. Its sealed terminal
binary hash matched the staged binary; release verification, Xfce health and
the live terminal PTY/resize/Ctrl+C test passed. Closing the active window
again ended its process cleanly. The VM package manifest now includes the font
for future images, but an image from that revised manifest has not yet been
built or booted.

The interactive shell test entered an 8192-byte line, observed the explicit
length error and then executed the following valid command. Temporary build
headers and `xclip` were removed after verification. No VM cold boot has yet
been run for this release.

## Remaining limits

X11 PRIMARY and CLIPBOARD are shared with other clients in this UID-1000
session; they do not protect secrets from untrusted applications. Selection
copies visible cell text, trims trailing spaces and inserts a newline between
selected screen rows; it does not yet reconstruct wrapped logical lines.
Pastes above 8192 bytes require a different workflow. The installed fallback
covers the tested CJK sample; broad Unicode coverage and wide-character cell
alignment remain unverified. Richer selection behavior, clipboard persistence
after the terminal exits and accessibility remain open. See
[security status](security.md) and the
[OS checklist](os-checklist.md).

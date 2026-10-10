# Heurism window-first workspace, 2026-10-10

The owner rejected the C desktop's four large application cards and system
dashboard as dated. Desktop 0.7 makes the desktop a work surface for real
windows. A persistent 48-pixel top panel shows the workspace, connection and
clock. The centered dock opens the app search, pinned apps, running windows and
quick settings. The search palette lists the built-in actions, current windows
and installed applications marked visible by GIO; launching those applications
uses GIO in the UID-1000 session. Launcher and quick settings use undecorated
windows. The generated Xfwm theme has quiet one-pixel borders and only the
minimize, maximize and close buttons. The C runtime remains C/X11/Xft with a
GIO dependency for the installed-app catalog. No Python or Tk process was added
to the active desktop.

The source is `userspace/native/heurism-desktop.c`; `Makefile` links GIO for
that executable. `tools/build-heurism-xfwm-theme.py` is a host-only asset
generator. The sealed theme archive is 3,969 bytes. `native_launcher_vm.sh`
now runs at 1280×800 or 1920×1080 and checks panel presence, launcher focus,
installed-app visibility, task switching, quick controls, settings and the
checked power entry. `native_desktop_dell.sh` uses F2, F3, F5 and F6 to test
the new C settings, device, network and sound routes without dashboard hit
coordinates. The Dell workspace test checks the panel, dock, launcher,
terminal and quick controls on an authenticated isolated Xvfb `:93` display.

The final sealed VM release is
`/opt/heurism/native/releases/heurism-os-20261010T160753Z-5683`. The isolated
1920×1080 pass succeeded before activation; preview frames are
`build/prime-vm/heurism-redesign-1920-workspace.png` and
`build/prime-vm/heurism-redesign-1920-quick.png`. The guarded installer
activated the final release. After removing the temporary build packages, a
normal VM reboot returned fresh boot
`faecd695-a4c4-4dc3-b8b5-6645e5b7ecea` with release health, UID-1000 C UI,
root SSH, watch, control, desktop and protected hashes passing. Checkpoint
`Heurism-modern-workspace-20261010` has UUID
`daa80285-d64f-415d-ad99-d5c4e904369e`.

On the physical Dell, the C desktop was compiled under strict warnings. The
Dell-only installer ran the Xfce recovery, shell and PTY, Dell control,
settings/device/network/sound, 1920×1080 workspace, Files/Editor and sealed
release corruption gates. It assembled
`/opt/heurism/native/releases/heurism-os-dell-20261010T161019Z-5092` and
activated it through its rollback gate. The live release reports desktop 0.7
under UID 1000 on existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`.
The top panel and dock exist on Xorg `:0` at 1920×1080. SSH, watch, C control
and desktop services are started; the eight protected hashes and C power check
pass. The live Xfce PAM screensaver process answered "active" on the
workspace's own session D-Bus. `BootCurrent` remains `0005`, `BootOrder` is `0005,0000`, and no
`BootNext` was shown by the read-only `efibootmgr` query. Build packages were
removed; installing them briefly updated Alpine's `pcre2` package from
10.48-r0 to 10.49-r0. The runtime stayed healthy after that removal.

This is a substantial shell change, not completion of the visual goal. The C
Files/Editor and hardware settings pages still use a basic Xft presentation;
touch ergonomics, notification behavior and accessibility need further work.
The Dell's physical screen and touch input were not observed in this pass,
and this exact Dell release has not been rebooted. The earlier firmware-logo
halt required physical power cycling, so no remote Dell reboot was attempted.
The shared X11 UID-1000 session and unencrypted root disk still limit
application isolation and data-at-rest security.

## Files follow-up, 2026-10-10

The C Files window now uses a places sidebar, a quiet location header,
file rows with size information, contextual Open/Rename/Trash actions, and a
status footer. Hidden files are off by default and Ctrl+H toggles them;
Ctrl+L edits the location. Ctrl+Shift+N creates a folder and Alt+Up visits
the parent. Text files open the C Editor, while other files use the user's
registered GIO application. Trash now creates the complete private directory
chain even for a new home directory.

The isolated VM interaction test created and renamed a folder, moved it to
Trash and restored it, opened a PNG through a test MIME association, opened a
text file in the C Editor, toggled hidden files and navigated by keyboard.
The sealed VM release is
`/opt/heurism/native/releases/heurism-os-20261010T162519Z-41556`; checked
reboot returned fresh boot `9691a9e4-e34b-405f-90e7-e60479cd5205` and
release health. Checkpoint `Heurism-Files-v03-20261010T1631` has UUID
`92097266-4e82-404a-a57b-3f2da2dddc56`.

The Dell-only assembler repeated the isolated C app and release gates. Its
guarded activation selected
`/opt/heurism/native/releases/heurism-os-dell-20261010T162805Z-22599`
on the existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`. Live UI,
root SSH, watch, control, release verification and protected hashes passed.
`BootCurrent` and `BootOrder` remained `0005` and `0005,0000`. Temporary
build packages were removed on both machines. The physical Dell was not
rebooted for this presentation change.

The next visual work should focus on fast keyboard and pointer search across
apps, windows, settings and local files; reliable window placement and
workspace switching; and consistent touch-sized controls in Editor and
Settings. These are specific usability gaps in the current build. Any
contextual assistance should be optional and show exactly what it reads or
changes. The shared X11 session and unencrypted disk still require separate
security work before treating installed apps as isolated.

## Local launcher search, 2026-10-10

The C launcher now finds visible files in the top level of Home and files or
folders in Documents, Downloads, Desktop and Pictures, including one nested
folder level. It scans at most 2,048 visible entries and keeps at most 256
results when the launcher starts. Search begins after two typed characters,
sorts file results by modification time, and matches names without opening or
reading file contents. Hidden entries and symbolic links are excluded. A
matching directory opens in C Files; plain text opens in C Editor; other file
types use the user's registered GIO application. Search results refresh when
the launcher is opened again. The bounds keep a large home directory from
stalling the desktop, while deeper files currently require Files navigation.

The isolated VM interaction test verified app and window search, opening a
visible document in C Editor, opening a matching folder in C Files, and hiding
a dotfile from results. The 1280×800 panel was captured at
`build/prime-vm/heurism-search-local-file.png`. The sealed VM release is
`/opt/heurism/native/releases/heurism-os-20261010T163403Z-33906` and survived
a checked reboot to fresh boot `2f6d7ffd-8cfc-4d08-ae02-c4e1dc093ba0`.
Checkpoint `Heurism-local-search-20261010T1640` has UUID
`be07e1d4-9941-4b81-a850-4db76873cfe4`.

The Dell 1920×1080 isolated gate verified local document search and C Editor
opening without changing display `:0`. The Dell-only assembler also passed
shell, control, desktop, app and sealed-release corruption gates. Its rollback
installer activated
`/opt/heurism/native/releases/heurism-os-dell-20261010T163754Z-22556`.
On existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`, live UI,
SSH, watch, control, release verification and protected hashes passed after
temporary build packages were removed. `BootCurrent` is `0005` and
`BootOrder` is `0005,0000`. No Dell reboot was performed.

The launcher still accepts only ASCII typed input, and the bounded scan is
not a full content index. Unicode input, search ranking, multi-workspace
navigation and consistent Editor/Settings layout remain open desktop work.

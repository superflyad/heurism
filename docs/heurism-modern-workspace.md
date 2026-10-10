# Heurism window-first workspace, 2026-10-10

## Product direction

Current desktop systems converge on a few useful patterns: windows and
workspaces that are easy to arrange, search that can open and act on work,
controls close to the task, and accessibility and security that are part of
the everyday interface. Apple's macOS Tahoe updates Spotlight actions and
its visual system; Windows 11 develops Snap layouts and contextual search;
KDE Plasma 6.4 develops flexible tiling; GNOME 49 improves quick settings and
accessibility. Sources: [Apple](https://www.apple.com/ca/newsroom/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/),
[Microsoft](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-snap-layout-menu),
[KDE](https://kde.org/gl/announcements/plasma/6/6.4.0/),
[GNOME](https://release.gnome.org/49/).

For Heurism, the design target is a calm, immediately usable workspace with
its own typography, color, iconography, motion and window chrome. Search should
find apps, local documents, settings and commands with explicit action names.
Windows should tile and move between spaces predictably. Built-in Files,
Editor, Terminal, Settings, lock screen and power controls should follow the
same interaction rules. Keyboard and touch must both work, with contrast,
focus and text size exposed as real controls. A dock or graphic treatment
alone does not meet this target. Prefer local, inspectable features over
collecting user activity or adding opaque AI services.

The current 0.7 screenshot still reads as an interim desktop: large empty
background, small symbolic dock tiles, limited window arrangement and no
visible indication of work in other spaces. The Editor improvement below
fixes a core task, but it does not by itself finish the visual redesign.

The implementation sequence is: finish core app interaction (Editor
selection, clipboard and find; Files keyboard and drag actions), establish a
shared C UI toolkit and visual tokens, improve tiling/space overview and
search, then bring Settings, lock screen and notifications into that system.
Keep the established Linux driver, Xfwm and Xfce recovery paths while the C
experience matures. The present X11 shared user session and unencrypted disk
remain security limits; the current look is an interim release.

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

## Four functional spaces, 2026-10-10

Both the VM and Dell already had four Xfwm workspaces, but the C panel showed
the fixed text "Workspace 1" and offered no switching. The panel now reads
`_NET_NUMBER_OF_DESKTOPS` and `_NET_CURRENT_DESKTOP` and displays clickable
numbered spaces with the active space highlighted. Super+1 through Super+4
switches spaces; Super+Shift+number moves the active window to that space and
follows it. The dock reads a task's `_NET_WM_DESKTOP` before activation, so a
task on another space brings its space forward. Xfwm remains the window
manager; the C shell sends EWMH requests and does not rewrite its state.

The isolated 1280×800 VM test verified panel clicks, keyboard switching,
moving a terminal to Space 2, returning to Space 1, reopening that terminal
from the dock and moving it back. The captured second space is
`build/prime-vm/heurism-workspace-2.png`. Existing launcher, search, settings
and quick controls passed in the same run. The sealed VM release
`/opt/heurism/native/releases/heurism-os-20261010T164527Z-42920` survived a
checked reboot to fresh boot `f209044e-a69c-49c3-8be5-5a38ae20919f`.
Checkpoint `Heurism-four-spaces-20261010T1650` has UUID
`4daeff76-55c6-4ff1-9092-571c679929cf`.

The Dell's isolated 1920×1080 gate proved the same panel, keyboard, move and
dock behavior without changing display `:0`. Its Dell-only assembly passed
the shell, control, hardware, apps and sealed-release corruption checks. The
guarded activation selected
`/opt/heurism/native/releases/heurism-os-dell-20261010T164918Z-26495`.
On existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`, the live UID-1000
workspace, SSH, watch, control, protected hashes and four-workspace EWMH state
passed after temporary build packages were removed. `BootCurrent` is `0005`,
`BootOrder` is `0005,0000`. No Dell reboot or live workspace switch was made.

Workspace occupancy and window previews are still absent. Shortcut grabs may
be unavailable if another window manager configuration claims the same keys;
the clickable panel remains available in that case. Touch ergonomics,
Unicode search input, Editor and Settings consistency, notifications and
accessibility remain open.

## C Editor 0.4, 2026-10-10

Editor now uses the same visual language as Files: a document header, clear
save state, numbered text rows and a compact line/column footer. The text
caret is visible and can be placed with a pointer. Long lines scroll
horizontally as the caret moves. Vertical movement counts UTF-8 characters
instead of raw bytes. Ctrl+Z and Ctrl+Y undo and redo edits; continuous typing
is grouped for up to two seconds, with a bounded 128-step history. Each Editor
process now owns a uniquely named draft and a held file lock; another Editor
cannot overwrite or recover its active draft. A fresh Editor recovers the
newest inactive draft. The previous single-draft file migrates on first open
and is removed only after the new draft is saved. Closing a window exits its
process cleanly. File and draft input reject invalid UTF-8 and embedded NUL
bytes before being shown.

The isolated VM Editor test covers pointer placement, save, undo, redo,
continuous typing, a long line, concurrent drafts, recovery and legacy draft
migration. The VM installer runs both Files and Editor interaction tests before
sealing a release. Captured Editor windows are `build/prime-vm/editor-initial.png` and
`build/prime-vm/editor-long-line.png`. The active VM release is
`/opt/heurism/native/releases/heurism-os-20261010T172320Z-20197`; checked
reboot returned fresh boot `e507e4db-871e-4de0-ab29-8637d3721833` with
release and service health. The VM has intentional `/etc/heurism/desktop-lock`:
pointer wake painted the password prompt in
`build/prime-vm/heurism-editor-final-lock.png`. This is lock-screen evidence,
not an unlocked desktop paint check. Checkpoint
`Heurism-Editor-migration-20261010` has UUID
`8e186c4c-ee4f-4eda-86d3-98a76f0cac5d`.

The Dell app gate uses a disposable home directory for Editor drafts. It
verified pointer placement, undo, redo and save on isolated Xvfb `:2`, then
the Dell-only assembler passed the broader shell, control, workspace, app
and sealed-release gates. The guarded installer activated
`/opt/heurism/native/releases/heurism-os-dell-20261010T172614Z-14907`.
On the existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`, live UI,
root SSH, watch, control, eight protected hashes, `BootCurrent` 0005,
`BootOrder` 0005,0000, `DriverOrder` 0000,0001 and absent BootNext passed
after temporary build packages were removed. The Dell was not rebooted or
physically viewed, so physical interaction and post-reboot persistence of this
release remain unverified.

Editor still lacks text selection, clipboard editing, find and replace, and
large-document indexing. Settings, notifications, accessibility and the X11
security boundary remain open desktop work.

## C Spaces view 0.8, 2026-10-10

The panel's Spaces label and Super+O now open a borderless workspace view. It
shows four space cards with window counts, approximate window positions and
clickable window titles. Selecting a window changes to its space and focuses
it. Number keys, arrows, Enter and Escape provide keyboard navigation. Small
occupancy dots in the panel indicate which spaces contain windows. The view
fits between the panel and dock at 1280×800 and is centered at 1920×1080.
Window previews are drawn from X11 geometry and titles; they are not live
window images. The current Xfwm configuration has four spaces.

The isolated VM test on `:95` moved a C terminal to Space 2, opened the view,
selected that window from Space 1 and verified focus and workspace state. It
also checked Super+O and keyboard selection. The full VM assembly gate and
existing launcher/search/quick-control regression test passed. Rendered
1280×800 and Dell-sized 1920×1080 evidence is
`build/prime-vm/spaces-with-window.png` and
`build/prime-vm/heurism-dell-spaces.png`. The final VM release is
`/opt/heurism/native/releases/heurism-os-20261010T174158Z-10289`; checked
reboot returned `fa45e0c4-2d36-4888-8516-6f44e24f9c17` with SSH, watch,
control, UI and release verification healthy. The intentional VM lock screen
painted after pointer wake. Checkpoint `Heurism-Spaces-view-final-20261010`
has UUID `de9edacf-a4d3-461c-8f35-f56c5583e916`.

The Dell-only gate tested the same interaction in isolated Xvfb `:93` at
1920×1080 without changing the live display. It passed the broader shell,
control, hardware, app and sealed-release gates. Guarded activation selected
`/opt/heurism/native/releases/heurism-os-dell-20261010T174526Z-9213` on the
existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`. Live release/UI,
SSH, watch, control, eight protected hashes, `BootCurrent` 0005,
`BootOrder` 0005,0000, `DriverOrder` 0000,0001 and absent BootNext passed.
Build packages were removed. The Dell was not rebooted or physically viewed;
physical touch acceptance and post-reboot persistence of this release remain
unverified.

The space view makes open work visible, but the desktop still needs better
window tiling, a shared app visual system, selection/clipboard in Editor,
accessibility controls and a coherent lock screen.

## Window placement, 2026-10-10

Each window row in Spaces now offers left and right placement. The C desktop
reads Xfwm's EWMH work area and the window's frame extents, rejects windows
whose declared minimum size exceeds a half-screen zone, then requests the
move and resize through the window manager. The panel, dock and window chrome
remain inside their reserved areas. Xfwm continues to manage focus and
workspace state; the shell has no privileged window-control service.

The isolated VM test moved a terminal across spaces, placed it on both sides,
placed Editor beside it, and asserted frame clearance above the dock at
1280×800. The rendered pair is `build/prime-vm/tiled-pair.png`. The Dell
isolated 1920×1080 test verified a terminal fit above its dock on display
`:93`; it left the active `:0` session untouched. Both complete assembly
gates passed, including the shell, control, apps and sealed-release checks.

The VM installer activated
`/opt/heurism/native/releases/heurism-os-20261010T180247Z-67108`. A checked
reboot returned fresh boot `6ab26f74-b594-4d59-8a6b-d8561772dde1` with
release verification, UID-1000 desktop, root SSH, watch and control healthy.
Checkpoint `Heurism-window-tiling-20261010` is
`bd7b3dfd-8d8f-46d0-a232-d47b933c17d1`. The Dell guarded installer
activated `/opt/heurism/native/releases/heurism-os-dell-20261010T180235Z-28708`
on the existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`. Its live
release, services and eight protected hashes passed after build packages
were removed. `BootCurrent` remained `0005`, `BootOrder` `0005,0000`,
`DriverOrder` `0000,0001`, and BootNext absent. No Dell reboot or physical
touch acceptance was performed.

The next target is a consistent interaction system across C apps: larger
touch controls, visible keyboard focus, Editor selection and clipboard,
Unicode search input, accessible settings and a coherent lock screen. The
current 27-pixel tiling buttons are too small for a final touch interface.
The shared X11 UID-1000 session and unencrypted disk remain security limits.

Recent desktop direction reinforces this sequence. Apple has expanded
Spotlight actions and desktop personalization; Windows has begun testing actions
directly in Search and describes explicit containment, identity and consent
for agents; KDE supports per-workspace tile layouts; GNOME 50 adds reduced
motion and stronger screen-reader behavior. Heurism should adopt the useful
interaction ideas while keeping actions transparent and user initiated:
[Apple](https://www.apple.com/ph/newsroom/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/),
[Windows Search](https://blogs.windows.com/windows-insider/2026/10/07/from-searching-to-doing-building-a-faster-more-streamlined-windows-search/),
[Windows security](https://blogs.windows.com/windowsexperience/2026/10/07/building-windows-for-hybrid-intelligence/),
[KDE](https://kde.org/gl/announcements/plasma/6/6.4.0/),
[GNOME](https://release.gnome.org/50/).

## Focused Spaces view, 2026-10-10

The four equal workspace cards made open work hard to read and forced window
actions into 27-pixel targets. Spaces now shows one selected workspace as a
large position diagram, with a separate list of its windows and four compact
space tabs. The 44-pixel left/right controls are easier to target. Selecting
a tab changes the preview; Open space changes the actual workspace. A window
title or diagram selects that window, including across spaces. The keyboard
still uses number keys for direct switching, arrows to choose a tab, Enter to
open it, and Escape to close the view. The map represents X11 window geometry
and titles; it does not claim to show live window contents.

The VM 1280×800 interaction test verified tab selection, cross-space window
activation, keyboard switching and a Terminal/Editor tile pair. The final
render is `build/prime-vm/spaces-focused.png`. The Dell isolated 1920×1080
test exercised the same navigation and tiling on `:93`, leaving its live `:0`
session untouched. Both release assembly gates passed the shell, control,
apps and sealed-release checks. The VM guarded activation selected
`/opt/heurism/native/releases/heurism-os-20261010T181435Z-31737` and a checked
reboot returned boot `df2dcd7d-7d25-4c98-9d2a-3221eaa0f804` with healthy
release/UI/SSH/watch/control. Checkpoint
`Heurism-focused-spaces-20261010` has UUID
`c11c98ff-a7a4-4ffc-95c5-88f5fa8ab191`.

The Dell guarded installer activated
`/opt/heurism/native/releases/heurism-os-dell-20261010T181420Z-8830` on the
existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41`. Live UID-1000
desktop, SSH, watch, control, release verification, eight protected hashes,
BootCurrent `0005`, BootOrder `0005,0000`, DriverOrder `0000,0001` and absent
BootNext passed after removing temporary build packages. No Dell reboot or
physical touch acceptance was performed.

The focused view is a real usability improvement, but a space with more than
five windows at 1280×800 or six at 1920×1080 currently shows an overflow
count without a way to reach the rest in this view. That needs scrolling or
search. The workspace diagram is still illustrative. Editor clipboard,
Unicode search, shared app styling and accessibility remain major work.

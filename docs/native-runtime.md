# Heurism C runtime

The active C releases were renamed and installed under `/opt/heurism/native`.
The VM and Dell both passed checked reboot into their new sealed releases;
see [heurism-migration.md](heurism-migration.md). The release IDs and command
names below record earlier Companion milestones and remain historical evidence.
The development VM currently runs
`/opt/heurism/native/releases/heurism-os-20261010T152041Z-51720` with
the C workspace, searchable C launcher, original Heurism window frames and
[interactive C shell job control](shell-job-control.md). See
[the visual pass](heurism-look.md) and [launcher](heurism-launcher.md). The Dell later
returned to pinned SSH on a healthy fresh boot after an unreachable interval; see
[the incident record](dell-terminal-20261009.md).
The Dell now runs the C workspace release
`/opt/heurism/native/releases/heurism-os-dell-20261010T153748Z-15862`; see
[the live-session record](heurism-dell-live-session.md).

Heurism is a Linux OS experience built from small Unix programs. Alpine owns
the kernel, drivers, OpenRC and standard utilities. Heurism owns the C shell,
graphical terminal, original workspace, settings, apps, control service and release verifier. Their
boundaries are process and Unix socket boundaries, like the shell and terminal:
the terminal handles display and PTY, while the shell interprets commands; the
desktop handles windows and input, while the root service checks privileged
operations. Files and Editor run as the desktop user and edit that user's data.

## Xfce desktop bridge, 2026-10-03

At this milestone the default user session on the VM and Dell ran Alpine Xfce
4.20 over the existing Xorg server. Both later selected the Heurism C workspace
with Xfwm window management; Xfce remains the Dell recovery session.
In the bridge session Xfce supplies the panel, window manager, desktop, Thunar
and Mousepad; Companion Settings is a regular C X11 window, and the C terminal
and shell remain available from the application menu and dock. The C control
service, hardware policy, sealed release verifier and root SSH/watch path remain
independent of Xfce. The original C workspace can be selected for recovery.
Details and validation are in [xfce-bridge.md](xfce-bridge.md).

The VM release at this 2026-10-03 milestone was
`/opt/companion/native/releases/c-os-20261004T004048Z-7138` and the active
Dell release then was
`/opt/companion/native/releases/c-os-dell-20261004T004134Z-8068`.
The preceding Xfce bridge release survived a checked reboot to fresh boot
`620b5571-88de-46b4-9576-0c43cbfc737d`: SSH, watch, C control, Xfce UI,
release verification, power checks and six protected hashes passed. The
1920×1080 framebuffer was painted. Saved touchpad settings and the actual
speaker sink returned. Dell firmware appended its known auto-created NIC
entries to `BootOrder`; after verifying them, the SSD order was restored to
`0005,0000`. `BootCurrent` is `0005`, `DriverOrder` is `0000,0001`, and
`BootNext` is absent. Temporary build packages were removed on both systems.
The power update adds a top-right Companion C Power launcher because Xfce's
ordinary Log Out dialog offered disabled Restart and Shut Down controls. The
session also replaces the disabled Log Out menu item with Applications >
System > Companion Power. The final sealed release and live Dell session show
that menu route opening the C Power window. The window requires two clicks
within ten seconds and uses the guarded C control socket. The preceding power
release restarted through the Dell UI to fresh boot
`2fe46192-7c97-4629-97f9-757032debc23` with healthy
management and desktop. On the VM, the UI restart and shutdown both completed;
Hyper-V observed the Off state after shutdown, then a host start returned
fresh boot `d383719e-9de0-4f3b-9886-9e29e522e0f9`. Dell shutdown was not
triggered because it would remove the only live SSH connection. The final menu
override release passed guarded activation and live menu navigation; it has not
received another physical reboot.

## Original C workspace release

`/opt/companion/native/releases/c-os-20261003T182145Z-17012` was active through
`/opt/companion/native/current` on the separate Hyper-V `CompanionDev` VM. It
was assembled from `userspace/native` with strict C compiler warnings, a SHA256
manifest and root-owned release files. The previous sealed C release and a VM host
checkpoint `Companion-before-C-runtime-20260930` (UUID
`ae8e79cc-ff44-4208-8756-d1b5fa35fb1f`) provide separate recovery paths.
The verified C baseline checkpoint is `Companion-C-runtime-ready` (UUID
`7e64ad35-6f70-4fac-826f-1b6e1b69c6b2`).

The control service listens only on `/run/companion-desktop/control.sock`. It
admits root and `companion-ui` by `SO_PEERCRED`, refuses unrecognized actions,
and requires the protected `hyperv-dev` platform configuration plus actual
Microsoft Virtual Machine DMI. Checked restart and shutdown verify the VM's
protected boot manifest, UEFI state, healthy boot ID, SSH and watch services,
and absence of `BootNext`. The administrator entry directs the user to
authenticated root SSH. The menu requires two clicks within ten seconds for a
power action.

The X11 workspace provides a persistent dock, window task buttons, desktop
shortcuts, Files, Editor, Browser, Terminal, Onboard, settings and honest VM
device labels. Files supports browse, folder creation, rename, Trash and
restore. Editor supports UTF-8 text, atomic save and draft recovery. The
desktop waits for Openbox, checks that its windows are visible and positions
the dock at the bottom before writing its health record. OpenRC, Xorg, the
window manager and POSIX shell startup glue remain Alpine components.

## Verification

- Native shell, terminal PTY, resize and Ctrl+C tests passed as UID 1000.
- Native control socket tests passed for root and UID 1000, appearance
  persistence, VM status and unsupported hardware responses.
- Files and Editor live X11 tests passed create, rename, Trash, restore and
  UID-1000 save. Dock launch and the root C terminal/shell passed in the
  original baseline release; the root X11 terminal was removed later.
- A failed activation returned to the previous C release. The Xorg lifetime
  and first-paint race found in that run were fixed before the final release.
- The final release passed `companion-release verify`, C service health and
  checked reboot. Fresh boot `ac14c8d9-a625-49e6-bb2a-21c09069b0bc`
  returned the same release, healthy SSH/watch/control/UI and a painted
  `capture --require-ui` framebuffer. Its dock geometry was `120,716`,
  `1040x68` on the VM's `1280x800` display. Temporary build packages were
  removed before reboot, and the live terminal test passed afterward.
- The 2026-10-03 VM release added cursor editing, session history and unquoted
  pathname globbing. Shell tests, a real interactive PTY test and the control
  socket test passed. A previous candidate was rebooted to fresh boot
  `88f67c54-14c3-47db-91ac-0a31f33a5dce`; the final glob candidate was
  activated and health checked with its rollback gate.

`tools/prime-vm.py wait` uses the C release verifier and C control status for
native boots. Its Python code is host-side VM orchestration; its legacy guest
check remains only for restoring an older checkpoint.

## Original C Dell release, 2026-10-03

The physical Inspiron 7506 2n1 ran the sealed C release
`/opt/companion/native/releases/c-os-dell-20261003T182623Z-27847` through
`/opt/companion/native/current`. It replaced the active Python/Tk desktop and
control service with the C desktop, C control service, C shell, C terminal,
Files and Editor. The legacy runtime remains sealed as recovery. The exact-DMI
`install-native-dell.sh` installer assembled and checked the release, then
activated it with a 20-second health gate and automatic rollback. It did not
replace the Dell boot loader, Linux kernel, network driver, EFI files, root
SSH or `companion-watch`.

The Dell C control service uses the same authenticated local Unix socket API
as the VM release. It reads real battery, AC, thermal, brightness and network
state; applies backlight and libinput touchpad settings; reads and restricts
Dell BIOS enum changes; and provides checked power actions. Its sound API
reads and adjusts the real PulseAudio speaker sink as `companion-ui`, including
separate left and right levels. Wi-Fi scan and connection are restricted to
`wlan0`. WPA PSKs are derived in C with OpenSSL PBKDF2 and written to an atomic
root-only configuration; no password is passed on a command line or saved in
plaintext. Connection waits for both WPA completion and an IPv4 address, then
restores the previous wireless configuration if either is absent. Ethernet
management is not reconfigured by this path.

Live Dell sidecar tests passed for root/UID-1000 socket access, backlight
change/readback/restoration, saved touchpad settings, speaker mute and stereo
volume readback/restoration, BIOS inventory and same-value readback, network
scan/status and invalid credential rejection. A deliberately nonexistent WPA
network failed after the bounded association wait; C removed its new config,
stopped the owned supplicant and left Ethernet IP, SSH, watch and protected
boot hashes unchanged. The C desktop was exercised on isolated Xvfb `:1`,
including settings, BIOS, Wi-Fi scan and masked password entry, and sound
pages. Files, Editor and the root C terminal passed live Xvfb `:2` interaction
in the original C release, with UID-1000 document ownership and root shell
identity. The root X11 terminal has since been removed. A deliberately
corrupted release clone failed manifest verification without changing the
active services. The C Dell Xorg config matches the running known-good config.

The first activated C release exposed a Sound-page crash in the live Dell
session: its page name table omitted Sound. The same fault was reproduced with
a core dump, then fixed with a complete table and compile-time count check.
The repaired release passed all assembly gates, activated cleanly, and opened
Sound with the same desktop PID before and after the action. A C checked reboot
returned fresh boot `ff299b08-abcb-43bf-9854-28d0e1c03ef9`, the same sealed
release, healthy SSH/watch/control/UI, painted 1920×1080 workspace and dock,
and a working UID-1000 PTY/resize/Ctrl+C terminal. Files opened from the live
dock; Firefox and Onboard launched from the C workspace. The real speaker sink,
libinput settings and BIOS inventory respond through the C control socket.
Six protected EFI/kernel hashes passed. `BootCurrent` is `0005`, `BootOrder`
was restored to `0005,0000` after Dell appended auto-created NIC entries,
`DriverOrder` is `0000,0001`, and `BootNext` is absent. Temporary compiler
packages were removed.

The 2026-10-03 usability and security release adds interactive cursor editing,
in-memory command history and basic pathname globbing to the C shell. The
control service now rejects `admin-console` for the desktop user, and the
desktop points administrators to authenticated root SSH. This closes the
direct route from any UID-1000 X11 client to an arbitrary root shell. The Dell
installer closes its flock descriptor when launching sidecar tests and OpenRC
services, so those children cannot hold the installer lock after activation.
The final candidate passed shell, PTY, socket, Dell hardware, desktop/app and
rollback gates. The active release survived a C checked reboot to fresh boot
`fcf8df45-34cb-4998-a56c-4f9a3f7bccc5`: SSH, watch, control, UI, release
verification, power checks and all six protected hashes passed. A live terminal
PTY/resize/Ctrl+C check passed on that exact release after reboot. The real
speaker sink and saved input settings remained available. Dell firmware
appended NIC entries during reboot; the verified SSD `BootOrder` was restored
to `0005,0000`. `BootCurrent` is `0005`, `DriverOrder` is `0000,0001`, and
`BootNext` is absent. Temporary build packages were removed.

The owner chose Ethernet as the active management link and deferred Wi-Fi
association. C Wi-Fi scan and failure rollback are verified; a real-password
association has not been observed. The owner reports that touch input and
speakers work on the physical Dell. Specific gestures and audio quality were
not described, and a real BIOS value change remains unobserved. The earlier Dell
restrictions on native kernels, PXE and management-link detachment still apply.

## Boundaries and next work

The Dell hardware code is restricted to the exact Inspiron 7506 2n1 DMI. The VM never
claims Dell Wi-Fi, speakers, touchpad or BIOS hardware. The Dell's existing
SSD boot/recovery and independent root management remain untouched. The shell
is not a full POSIX script shell; BusyBox ash still runs startup scripts. A
local C terminal candidate has bounded scrollback and passed real X11
keyboard/wheel, long-output and resize checks in the isolated image VM. The
current development VM also has [text selection and clipboard](terminal-clipboard.md)
with live PTY evidence. The Dell ran the earlier scrollback release before a
reboot whose fresh boot was later verified; accessibility remains open. Browser
and Onboard are external
Unix applications. Companion remains a C userspace on Linux, with the Dell
release active under the checks above. See [security.md](security.md) for the
current threat model and remaining security work.

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

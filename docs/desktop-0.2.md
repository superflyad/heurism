# Companion local shell 0.2 (historical)

This records the earlier shell. See [current desktop](desktop.md) and
[workspace verification](workspace.md) for the installed 0.3 release.

The first Linux-based Companion interface is installed on the physical Dell.
It starts automatically on virtual terminal 7 at 1920x1080. The native Python/Tk
application runs as `companion-ui` (UID 1000), with a separate root control service.
Xorg uses the existing Linux Intel modesetting driver and libinput for the
explicitly discovered keyboard, touchpad and touchscreen event devices. Input
metadata is populated for those devices using scoped `udevadm test` invocations;
this writes input classification records/symlinks and does not execute RUN rules.
The eudev package also supplies a settle provider that OpenRC pulled into the
normal boot, starting udev. The existing mdev sysinit entry remains; udev owns
the running hotplug handler after startup. No network configuration, EFI loader
or protected kernel files were changed. Root SSH and recovery remain available.

## Implemented behavior

- Live IPv4/interface, battery/AC, memory, uptime, thermal and BIOS/model readings.
- Real backlight brightness controls with a 5–100 percent range.
- Night/light appearance persisted on disk and restored after reboot.
- Automatic UI recovery through an independent OpenRC supervisor, with bounded
  respawn attempts. A failed UI does not shut down SSH or the recovery watch.
- Overview, Input and Device pages, navigable by touch/pointer or Tab and Enter.
- Live, saved tap-to-click, natural scrolling and pointer-speed settings applied
  to the real libinput touchpad. Two-finger scrolling, tap-and-drag and typing
  rejection are enabled in the X session.
- A drawing area accepting pointer-emulated touch drags, plus a scrolling sensor
  list accepting wheel/two-finger scroll events and one-finger drag scrolling.
  Drawings survive periodic status refreshes. Pressing a button and dragging away
  cancels the action rather than changing settings.
- F1/F2/F3 select pages; Escape returns to Overview; F5 refreshes.
  Ctrl+Q closes the UI and the supervisor restarts it.
  Ctrl+Alt+F2 selects the existing login console; Ctrl+Alt+F7 returns to the shell.

This is an initial shell, not a complete desktop/application suite.
Wi-Fi configuration, BIOS setting editing, applications, suspend and system power
actions are not exposed in this UI yet. Startup into Xorg works, but continuous
graphics through the firmware/loader/Linux display transitions is not proven.

## Local control boundary

The UI communicates over `/run/companion-desktop/control.sock`. There is no TCP
API listener. Its parent directory is root-owned and mode 0750, the socket 0660,
both in the `companion-ui` group. The server additionally checks Linux peer UID
credentials and accepts only root or the UI account. Requests are newline-framed
JSON limited to 4096 bytes, version 1, with explicit `status`, `brightness`,
`theme`, `input-status` and `input-settings` actions. Input settings accept only
named boolean switches and a finite speed in [-1, 1]. They apply fixed xinput
properties to the named touchpad, verify readback, and persist after success;
partial application failures attempt restoration. It provides no arbitrary command, filesystem or firmware-write
operation. State reads have bounded subprocess timeouts; display polling occurs
off the Tk event thread.

Appearance and input settings are stored at `/var/lib/companion/desktop/preferences.json` using an
atomic file replacement. Brightness changes are live only; saved brightness
across reboot is not claimed. Xorg is privileged to access the display and input;
the UI itself is unprivileged. X authorization cookies restrict local clients,
and Xorg's TCP listener is disabled.

## Deployment and recovery

Packages installed: `xorg-server`, `xinit`, `xauth`, `xf86-input-evdev`,
`python3-tkinter`, `font-dejavu`, `xwd`, `xdotool`, `xrandr`, `xvfb`, plus dependencies.
The display architecture follows Alpine's [Xorg workflow](https://wiki.alpinelinux.org/wiki/Xorg)
and its [Tk package](https://pkgs.alpinelinux.org/package/v3.24/main/x86/python3-tkinter).
Version 0.2 additionally installs `xf86-input-libinput`, `libinput-tools`, `eudev`
and `xinput`. Settings follow the upstream [libinput configuration contract](https://wayland.freedesktop.org/libinput/doc/latest/configuration.html)
and [Xorg input properties](https://cgit.freedesktop.org/xorg/driver/xf86-input-libinput/tree/man/libinput.man).
The display/input configuration is generated for this inventoried Dell; portable
device discovery and touchscreen pinch/multifinger gestures remain later work.
The Tk shell consumes touchscreen pointer emulation, not a full multitouch API.

`python tools/stage-desktop.py` authenticates through the guarded Dell helper,
uploads an archive with pinned SSH/SCP trust and verifies its hash. Run the staged
`install.sh` to install into `/opt/companion/desktop` and `/etc/init.d`; it records
backups and runs control boundary tests without enabling services. Services were
then explicitly added to the default runlevel and tested on hardware.

Backups for this iteration are under `/var/lib/companion/desktop-backup-*`.
The first installation backup is `desktop-backup-20260927T184018Z`.
To disable the graphical shell over trusted root management:

```sh
rc-service companion-desktop stop
rc-update del companion-desktop default
chvt 1
```

This leaves root SSH, the recovery watch and login consoles available. Control
can also be stopped/disabled separately if the desktop is stopped. No EFI or
kernel replacement is needed to roll back this interface.

## Verification

Five Linux host cases cover invalid operations/frames, brightness rejection and
write bounds, preference persistence, and unsafe input values rejected before
device commands. Xvfb runs the
actual unprivileged Tk application against the live control service, verifies a
pointer-triggered preference change, keyboard refresh/exit, invalid API requests
and management remaining healthy after exit.

The input interaction test runs the actual Tk shell as UID 1000 under Xvfb,
verifying keyboard navigation, a held-press button surviving refresh, cancellation
when dragging away, persistent drawing across refreshes, wheel and finger-style
drag scrolling, and device-page navigation. Its tap setting action is verified
against the real Dell xinput property and saved preferences, then restored.

The physical check verifies the real backlight write/read/restore, a pointer
action on the real X display, an intentionally terminated UI automatically
returning with a fresh PID, and management remaining healthy on the same boot.
These injected events establish software interaction; a person physically using
the touchscreen/touchpad has not yet been observed. The hardware input devices
were successfully opened by Xorg. Console switching to tty2 and back to tty7 also
passed over management.

A normal reboot returned root SSH and both desktop services on boot
`05383219-e31e-4bd9-9dae-8f2e48d6debc`, with tty7 active and the saved light theme
still present. Night appearance was restored afterwards. BootCurrent is 0005;
BootOrder was restored to 0005,0000 after Dell appended NIC entries. DriverOrder
remains 0000,0001, BootNext absent, all six protected EFI/kernel hashes unchanged.
This is SSD-dependent startup, not an independent firmware reset channel.

Version 0.2 returned UI and healthy root management on normal boot
`02e13d27-8e55-4f10-8da0-c41d50ccdce3`. Saved natural-scroll=false and speed=0.2
were reapplied to the real libinput device after reboot; natural-scroll=true and
speed=0.0 were restored afterwards. BootCurrent 0005, BootOrder restored 0005,0000,
DriverOrder 0000,0001, BootNext absent; all protected hashes unchanged.
SSH was reachable during early OpenRC startup, before UI/watch were ready; later
checks verified all four services and tty7, rather than treating an open SSH port
as complete startup. Physical human touch acceptance is still unobserved.

Local evidence is under `build/desktop/`: source hashes, API/UI/input integration JSON,
physical check JSON, reboot state, post-reboot installed-source/boot/hash checks,
and `companion-desktop.png` captured from the actual Dell display. Use
`tools/collect-desktop-evidence.py` and `tools/capture-desktop.py` for fresh records.

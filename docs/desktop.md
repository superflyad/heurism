# Companion desktop 0.3

Companion starts automatically on the Dell's 1920×1080 display at tty7. Alpine
Linux supplies the kernel and hardware drivers. The desktop runs as
`companion-ui` (UID 1000), with a separate root control service and independent
SSH/recovery services. See [workspace workflows and proof](workspace.md).

## Local experience

Home launches Files, Editor, Firefox, Terminal, Onboard, Network, Sound and BIOS
settings. Switch apps restores open or minimized windows, including the
Administrator terminal. Windows+D returns to the interactive Home workspace;
Alt+Tab switches applications. Internal application windows also have Home,
Keyboard and Close buttons. Openbox handles movement, resize, maximize, minimize
and deliberate closing.

Overview shows live network, battery/AC, memory, uptime and temperature, plus real
brightness and saved appearance controls. Input provides saved tap-to-click,
natural scrolling and pointer speed, with a drawing/scrolling area. Device shows
Linux/firmware inventory. F1/F2/F3/F4 select Overview/Input/Device/Home; Escape
returns Home and F5 refreshes. Ctrl+Alt+F2 selects the existing console;
Ctrl+Alt+F7 returns to Companion. Ctrl+Q closes the shell and the supervisor
starts a fresh session.

The Dell keyboard, `DLL0945:00 06CB:CE27 Touchpad` and
`CUST0000:00 04F3:2A4B` touchscreen are explicitly discovered and opened through
Xorg/libinput. Two-finger scrolling, tap-and-drag and typing rejection are enabled.
Scoped `udevadm test` calls populate metadata for these input devices.
Touchscreen pointer emulation reaches Tk; raw multitouch/pinch and automatic
tablet rotation are not implemented. Physical finger/trackpad acceptance remains
distinct from injected X events and verified real driver properties.

Intel SOF firmware, ALSA UCM configuration and PulseAudio expose the speaker.
Cold-boot ALSA nodes created before udev starts need card classification: the
session prepares only discovered sound cards with `udevadm test --action=change`.
This records `SOUND_INITIALIZED` and metadata; queued RUN rules are not executed.
No Ethernet driver or USB configuration is detached or changed. Silent PCM
playback to the real speaker sink is verified; audible quality is not claimed.

## Control interface

The UI uses `/run/companion-desktop/control.sock`, mode 0660, inside a root-owned
mode-0750 directory. Linux peer credentials allow root and UID 1000. Xorg's TCP
listener is disabled and X cookies restrict clients. This is a trusted owner
administration session with shared X access, not an application isolation sandbox.

Requests are newline-framed JSON, version 1, limited to 4096 bytes. Named actions
provide status, brightness, appearance, input settings, Wi-Fi scan/configuration,
BIOS attribute reads and allowlisted keyboard settings, a fixed Administrator
console, and confirmed reboot/shutdown. There is no arbitrary command field or
TCP API. Brightness is constrained to 5–100%; input values are validated and
read back from xinput. Preferences use atomic replacement at
`/var/lib/companion/desktop/preferences.json`. Brightness remains live-only.

Wireless configuration passes the password on stdin, removes plaintext comments
from the WPA file and saves `/etc/companion/wifi.conf` as root mode 0600.
Reconfiguration verifies wireless daemon ownership; an existing owned DHCP client
is renewed rather than duplicated. Startup/watch services load saved Wi-Fi
configuration. Scanning and modeled transaction failures pass; association with
the owner's real password has not been tested.

BIOS editing here is limited to Fn Lock, Fn Lock mode, keyboard illumination and
AC/battery keyboard backlight timeouts. Other supported attributes are view-only.
The existing root `companion-bios` tool remains available.

Power actions check all six protected boot/kernel hashes, the SSD management and
fallback entries, DriverOrder and absence of BootNext. If Dell appended only
verified automatically generated MAC/IP network entries after the proven SSD
prefix, the action records the previous order and restores `0005,0000`.
Unexpected entries/orders fail without reboot. No network entry is selected.

## Releases and recovery

Root-owned releases live under `/opt/companion/releases/`; the active desktop is
an atomic symlink at `/opt/companion/desktop`. SHA256 manifests require expected
files and reject modified, symlinked or user-writable code.
`/usr/local/sbin/companion-release` stays outside the candidate release so a
broken candidate cannot remove the fallback helper.

The client verifies integrity, then waits up to 15 seconds for a live UID-1000
shell, current release/boot and successful API state. A failed candidate restores
the previous release. OpenRC retries X with bounded respawns. SSH and the recovery
watch do not depend on the UI. A failed cloned UI physically demonstrated
automatic return without owner input; this is UI rollback, not a complete Alpine
package transaction rollback.

For manual rollback over trusted root management: stop `companion-desktop`, run
`python3 /usr/local/sbin/companion-release rollback`, restart `companion-control`,
then start `companion-desktop`. Stopping/disabling only the desktop leaves SSH
and SSD rescue intact. Backups are under `/var/lib/companion/desktop-backup-*`.

Current physical boot is `a51e7782-1f35-4e31-814c-0d4b4e7b14ef`, release
`20260927T195311Z-7003`. All four desktop/management services are started, tty7 is
active, BootCurrent is 0005, BootOrder is 0005,0000, DriverOrder is 0000,0001 and
BootNext is absent. All protected hashes are unchanged. Input/appearance settings,
a user-owned document and the real speaker sink survived normal reboot.

Startup still depends on the SSD and Dell firmware. Ethernet SSH cannot control
a firmware error screen or provide an independent reset channel. See
[historical 0.2 evidence](desktop-0.2.md) for the earlier input installation.

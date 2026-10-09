# Companion product roadmap

Linux stays as the kernel and hardware foundation. The next work builds the
Companion experience and hardware interfaces on the installed Alpine system.
Custom kernel and native driver experiments are optional research, not deployment
gates. See [architecture](architecture.md).

| Milestone | Deliverable | Evidence required | Current state |
| --- | --- | --- | --- |
| Management foundation | Persistent root management, internal rescue and supported BIOS tools | Physical boot without USB, authenticated reconnect and recorded recovery behavior | Installed and verified; SSD dependent |
| Startup presentation | Clean transition into Companion with deliberate recovery access | Physical startup observation, hidden menu recovery and management reconnect | Hidden GRUB and minimal local presentation verified; graphical transition pending |
| Control service | Structured live inventory and specific hardware/system actions | Real Dell readings, validated requests and verified action results | Status, brightness, appearance, input, Wi-Fi, supported BIOS and checked power API installed |
| Local shell | Automatically started full-screen Companion interface | Actual keyboard/mouse/touch interaction, live state and UI restart without losing SSH | Version 0.3 installed; physical X input and Onboard typing pass; human finger/touchpad observation pending |
| OS workflows | Settings, application/session lifecycle and persistent preferences | Complete local tasks and persistence across restart | Files, recoverable Trash, editor/drafts, Firefox, terminals, keyboard, window switching and settings implemented; see workspace evidence |
| Updates and recovery | Versioned installs, health checks and rollback for Companion services/UI | Failed update recovers to working services and management | Root-owned releases, integrity checks and initial UI health gate installed; failed cloned UI automatically restored the working physical session |
| Platform behavior | Battery, charging, thermal, networking and suspend/resume controls | Measured physical behavior and reconnect after supported transitions | Inventory available; feature acceptance pending |
| Portability | Same service/UI contracts on another supported Linux target | Product workflows pass on second hardware target | Later |

## Next product milestone

The [first local shell and control service](desktop.md) now run on the Dell, using
Tk/Xorg on Linux. Live hardware/network/power state, brightness and appearance
controls are implemented. Automatic startup, appearance persistence across reboot
and UI respawn independently of root SSH passed physical checks.

Keyboard navigation, saved libinput settings, document/application workflows and
automatic failed-UI rollback are implemented. See [workspace verification](workspace.md).
Further platform acceptance covers physical finger gestures, audible output,
Wi-Fi association with owner-entered credentials, tablet rotation and safe
suspend/resume. These do not require replacing Linux or the motherboard firmware.

Acceptance requires real Dell state, responsive local input, management surviving
a UI restart, persisted settings and a normal reboot returning to Companion and
SSH. Preserve the known boot entries, protected files and internal rescue. A clean
startup does not prove control during a firmware halt.

## Research retained

The original UEFI screen ran on the Dell. The freestanding kernel, memory/input
and native Ethernet/USB experiments have recorded host/VM evidence. None of the
native drivers is installed on the Dell. Historical reports apply to their pinned
sources; later edits need their own verification. See [kernel research](kernel-plan.md).
Firmware replacement and SSD-independent startup are not current product goals.

# Companion workspace 0.3

This is the historical Python/Tk workspace verification record. The active
Dell desktop is the [C runtime](native-runtime.md); its current
[security status](security.md) supersedes the administrator-terminal workflow
below.

The usable desktop foundation is installed on the Dell. Linux remains the kernel
and driver platform. Active release: `20260927T195311Z-7003`. Verified normal boot:
`a51e7782-1f35-4e31-814c-0d4b4e7b14ef`, 2026-09-27.

## Workflows

| Workflow | Delivered behavior | Evidence |
| --- | --- | --- |
| Files | Browse, open text, create folders, rename, Trash and restore without overwriting an occupied destination | Real Tk/user create, rename, Trash/restore and content checks |
| Editor | UTF-8 files up to 4 MiB, New/Open/Save/Save as, undo, Ctrl+S/O/N, atomic private writes and draft recovery | Actual edit/save/read, preserved 0600 mode and recovery after losing the document window |
| Browser | Firefox with a local address/search start page | Actual Firefox windows in isolated and physical display sessions |
| Terminal | Ordinary Linux shell as UID 1000 | Typed `id -u` and resulting file on the real display |
| Administrator | Explicit root terminal after local confirmation | Fixed API launch, real UID-0 process and typed UID readback |
| Touch typing | Onboard large keys and D-Bus Show for reopening | Clicking the actual Onboard Q key entered `q` in a real text field |
| Windows | Movement, resize, maximize/minimize, Alt+Tab, Home and Switch apps | Actual minimize/restore and switcher inventory; physical app launches |
| Input | Saved touchpad tapping, natural scroll and speed; touchscreen pointer emulation | Live libinput readback, press/redraw, drag cancellation, drawing, scrolling and reboot persistence |
| Network | Ethernet status and WPA Wi-Fi dialog, private saved configuration and verified daemon ownership | Actual scan with Ethernet healthy; modeled wireless transaction cases |
| Sound | Real speaker sink, volume and mute | Volume/mute write/read/restore, cold-boot detection and real sink running during silent PCM playback |
| BIOS | Supported attributes and allowlisted keyboard presentation changes | Real Dell attribute inventory, writable/view-only distinction and invalid-write rejection |
| Power | Deliberate restart/shutdown with boot protection checks | Normal API reboot, fresh boot ID, UI/root reconnect and persisted state; shutdown implemented but not executed |
| Recovery | Versioned releases, integrity checks, initial UI health and fallback | Isolated integrity failures and physical broken-clone rollback without owner input |

Windows+D or an internal application's Home button returns to Home. Switch apps
restores an open/minimized application. Keyboard opens/shows Onboard. Files starts
in the session user's Documents. Dirty editor drafts save every 1.5 seconds;
the next editor offers recovery. Save before a deliberate power action.

Tk, Openbox, Firefox, xterm, Onboard and PulseAudio come from Alpine packages.
The window manager follows upstream [Openbox bindings](https://openbox.org/help/Bindings)
and [actions](https://openbox.org/help/Actions). Companion's Home helper minimizes
application windows and focuses the interactive desktop without enabling the
ShowDesktop input overlay.

## Verification

Six control-boundary cases, eight wireless/power-gate cases and five isolated
release-integrity cases pass. `build/desktop/` records installed/source hashes,
service and boot checks, the actual `companion-workspace.png` display capture and:

- `workspace.json`: documents, files, applications, audio, BIOS and Wi-Fi scan.
- `physical-apps.json`: real display launches, terminal UID proof and Onboard typing.
- `input-interaction.json`: gesture/navigation tests against the live API.
- `physical-check.json`: backlight, pointer action and UI crash/respawn with healthy management.
- `physical-rollback.json`: a sealed cloned UI failed execution; the supervisor
  automatically restored the working release without reboot or owner input.
- `workspace-reboot.json`, `reboot-state.json`, `input-reboot.json`: fresh normal
  boot, persisted document/preferences, tty7 and real speaker.
- `audio-playback.json`: unprivileged silent PCM ran the actual speaker sink.
- `post-reboot.txt`: four services, tty7, boot/driver orders, protected hashes,
  installed source hashes and current release health.

Tests use the guarded Dell helper and pinned host identity. Staging verifies its
archive hash. `tools/collect-desktop-evidence.py` and `tools/capture-desktop.py`
refresh records. Physical reboot/rollback tests require explicit invocation and
never run unexpectedly during installation.

The first 0.3 reboot exposed a PulseAudio null sink despite loaded SOF firmware.
Missing cold-boot udev sound-card classification was identified and corrected.
The next reboot selected the real speaker automatically. No firmware, EFI loader,
kernel or recovery image was replaced.

## Acceptance limits

Injected X events and live libinput properties prove software paths; physical
finger/trackpad movement has not been observed in this iteration. Audible quality,
real-password Wi-Fi association, suspend/resume, tablet rotation and full
multitouch gestures remain separate acceptance work. Browser startup was verified;
arbitrary websites were not exhaustively tested. UI rollback does not reverse all
Alpine packages or recover storage loss. Tested UI failures retain root management,
and normal reboot returns it; a firmware halt still needs local intervention.

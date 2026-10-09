# Dell daily-use acceptance pass

This is the first physical pass for the [OS checklist](os-checklist.md). A remote
service check proves the system is reachable; it does not prove that someone can
use the display, keyboard, touchpad or speakers comfortably. Record the first
failure in each workflow before changing the system. Keep root SSH, watch, the
SSD boot path and the working desktop available during the pass.

## Remote baseline, 2026-10-09 15:49 UTC

Read-only checks used `tools/dell.py` with the pinned SSH host identity. No
service, firmware, boot entry or user file was changed.

| Check | Observation |
| --- | --- |
| Boot ID | `4dc58594-e232-44f6-baa2-6998c17115cb` |
| Active C release | `/opt/heurism/native/releases/heurism-os-dell-20261009T020254Z-7718` |
| Release and UI health | Sealed hashes verified; UID-1000 `xfce4-session` health record matched the active release and boot |
| Services | SSH, watch, boot health, C control and desktop running; no OpenRC crashed service reported |
| Display | Internal `eDP-1` connected at 1920×1080, 60.05 Hz; Xorg, Xfce window manager, panel and desktop processes running |
| Apps | Thunar, Mousepad, Firefox, Onboard and Xfce terminal packages present; Heurism C terminal points into the active release |
| Hardware status | Ethernet active, AC online, battery full, sound devices present; these are device observations, not audible or touch acceptance |
| Boot state | `BootCurrent 0005`, `BootOrder 0005,0000`, `DriverOrder 0000,0001`; no `BootNext` reported |
| Storage | Root filesystem about 1% used; 440.6 GiB available |
| Session lock | `xflock4` wrapper is present, but no supported locker package or process was found. There is no verified lock/unlock path. |

Once the laptop is reachable, the first hands-on pass can run without a planned
power cycle. A later cold boot must repeat it to satisfy the checklist's full
acceptance criterion.

**Update:** a [terminal release activation](dell-terminal-20261009.md) passed
on this boot, but its checked reboot did not return to remote management.
The baseline above is historical; the Dell's present state is unverified.

## Hands-on pass

Use the Dell's own display and input. Save a disposable document under the
normal user's Documents folder. Do not put a password or private document in
the test record. For each row, record **pass**, **fail** or **not tried**, plus
the exact action and visible result. A remote X11 injection cannot mark a row
as passed.

| Workflow | Steps to try | Result |
| --- | --- | --- |
| Start and navigate | Open the lid; note whether the desktop is painted and responsive. Open Applications, switch between two windows, minimize and restore one. | Not tried |
| Files and editor | In Thunar, create a test folder. In Mousepad, save a short UTF-8 document there, close it, reopen it and check the bytes. Rename the file, move it to Trash and restore it. | Not tried |
| Browser | Open Firefox, load a site you choose, download a disposable file and find it in Thunar. Check the default opener. | Not tried |
| C terminal | Launch **Heurism C Terminal**. Run `id -u`, `pwd`, `printf 'hello\n'`, a pipeline and a command that fails. Resize the window; run `sleep 30` and stop it with Ctrl+C. Try copying text and pasting a long line. Record each failure separately. | Not tried |
| Input | Type on the keyboard, click and scroll with the touchpad, tap the screen, use Onboard, and try tablet posture. Note missed taps, pointer jumps and wrong orientation. | Not tried |
| Sound and display | Play audible content, change volume and mute, check headphones and microphone, and adjust brightness. Check scaling and rotation if those controls are shown. | Not tried |
| Settings | Open Sound, Input, Network and Power. Compare each displayed state with what the hardware does; change one harmless preference and check it after desktop restart. | Not tried |
| Power | Open **Heurism Power** and verify Restart and Shut down are clearly labeled and require confirmation. Execute them only as a separately planned live test with recovery access available. | Not tried |
| Lock | Blocked: no verified local locker or login boundary is installed. Do not count an Xfce menu item or `xflock4` executable as a working lock. | Blocked |

### Issue record

For each problem, write: workflow; exact steps; expected result; actual result;
whether it repeats; screenshot if useful; boot ID; active release. Mark a
failure **P0** if it loses work, prevents login/recovery, exposes an unlocked
session or stops an ordinary task. Mark other reproducible daily-use failures
**P1**. Keep observations separate from proposed fixes.

## Acceptance gate

This pass is complete only when the hands-on rows have real results, the local
session lock has been implemented and tested, and a cold-boot repeat confirms
the same workflows. A pass must also leave SSH/watch/control healthy, the
sealed release verified and the known SSD boot/driver orders intact. See
[security status](security.md) for the current shared-X11 and unencrypted-disk
limits, which this workflow test does not resolve.

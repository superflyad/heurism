# Dell C terminal update and reboot status, 2026-10-09

This record separates a successful live release activation, an initial loss of
remote access after reboot, and later verified recovery through pinned SSH.

## Release activation

The source change at `9194a85` added a 512-line bounded scrollback ring to
`heurism-terminal`, with Shift+PageUp/PageDown and wheel navigation. The same
source built under strict C flags in `CompanionDev`, passed its sealed-release
gate and the real UID-1000 PTY, resize, pipeline and Ctrl+C test. The compiled
terminal SHA256 was
`a50f54d4a95552336bdf0001f64caa8df3da15c6bd572c35f47f89e911ed77b8`.

The exact binary was copied through the pinned Dell SSH connection into its
root-owned mode-0700 stage. The Dell installer assembled release
`/opt/heurism/native/releases/heurism-os-dell-20261009T155551Z-25889` after
the Xfce, shell, control, desktop, apps and bad-manifest sidecar checks. Its
guarded activation passed the health gate and left SSH, watch, control and
Xfce running on boot `4dc58594-e232-44f6-baa2-6998c17115cb`.

A live Dell terminal under UID 1000 ran `id -u` and returned `1000`. After
`seq 1 700`, the window capture changed on Shift+PageUp and returned to the
byte-identical capture on Shift+PageDown. The terminal was closed and its
temporary files removed. The active release verified, its UI health matched
the boot, protected files passed their pinned hashes, and firmware variables
were `BootCurrent 0005`, `BootOrder 0005,0000`, `DriverOrder 0000,0001`, with
no `BootNext` reported. These observations all preceded the reboot.

## Reboot did not return to remote management

The C `power-check` succeeded and accepted a confirmed reboot. The pinned
Dell SSH identity did not return within the initial 180-second reconnect
window. One local Wake-on-LAN packet and a further 120-second wait also found
no trusted SSH target on the configured subnet. The host's own Ethernet and
gateway were reachable, while the Dell's last address gave no ping or ARP
response. The cause is unknown: remote evidence does not distinguish a power,
firmware, boot, cable or network failure.

No further power, firmware or boot-order operation was attempted. At that
point the new boot ID, installed release health and protected-file hashes
were **unverified**. Closed-lid `s2idle` wake is previously proven, but it did
not restore access in this attempt and never established wake from full
poweroff or a firmware halt. The existing SSD rescue and root management paths
were not deliberately changed. The next trustworthy check requires local
power/display observation or the Dell returning to its pinned SSH identity.
The remote helper now rejects known power commands unless an operator explicitly
asserts local physical recovery is available. This guard prevents the same
unattended command path from being used accidentally; it cannot parse every
possible shell script or provide an independent reset channel.

## Trusted access restored later on 2026-10-09

After the owner reported that the Dell should be reachable, `tools/dell.py`
authenticated the pinned root SSH host at `10.8.22.238` on fresh boot
`30913da8-5441-41d6-967f-020680316816`. The cause and exact recovery path
from the earlier unreachable interval remain unknown; this does not prove the
earlier unattended reboot recovered by itself.

The same sealed Dell release
`/opt/heurism/native/releases/heurism-os-dell-20261009T155551Z-25889`
verified and reported a healthy UID-1000 Xfce session on tty7. SSH, watch,
C control and desktop services were started. All six protected EFI/kernel
hashes plus the two Heurism identity hashes matched. `heurismctl status`
reported management healthy, AC online and Ethernet at `10.8.22.238`.
An XWD capture using `/run/heurism-desktop/Xauthority` showed the painted
1920×1080 Xfce desktop and dock. `BootCurrent` is `0005`, `DriverOrder` is
`0000,0001`, and `BootNext` is absent. Firmware appended its known
auto-created USB NIC entries: `BootOrder` is `0005,0000,0001,0002`; the
proven SSD entries still lead. No firmware variable, service, power state or
active release was changed during this verification. The remote-power guard
remains in place; no further reboot was attempted.
The read-only C `power-check` passed with the appended NIC entries. The speaker
sink was present but muted; this check does not establish audible output.

## Firmware-logo stall and mitigation

The owner subsequently reported finding the Dell stuck at its opening logo
while the laptop was throttling. They turned power off and on; it then booted
normally. The persistent syslog shows orderly OpenRC shutdown at 15:57:26 UTC
and no Linux startup log until 23:20:27 UTC after that physical power cycle.
There is no evidence of an intervening Linux boot. This places the observed
stall before the installed services could restore SSH. The exact firmware or
early-boot fault, and any warning text hidden by the logo, remain unknown.
The terminal release installer changed no EFI, firmware or kernel file; the
reboot exercised the existing preboot path.

An earlier cosmetic change had set Dell BIOS `WarningsAndErr=PromptWrnErr` to
hide its headless banner. [Dell states](https://www.dell.com/support/kbdoc/en-us/000139731/what-the-headless-operation-mode-active-post-message-means-and-how-to-stop-it-appearing-during-start-up)
that this setting stops POST on warnings or errors. It is a plausible
contributor, not a proven cause of this particular blank-logo stall. On the
recovered boot, `companion-bios set WarningsAndErr ContWrn` restored the prior
continue-on-warning value through Dell's supported firmware interface. The
tool logged the old and new values; readback is `ContWrn` and
`pending_reboot=1`. The sealed release, UI, SSH and watch remained healthy on
the same boot. The new policy had not yet been tested through a restart at
that point. The remote-power guard stayed in place.
On the recovered Linux boot, three readings two seconds apart showed CPU
package temperature falling from 42°C to 41°C and package thermal-throttle
count stable at 42. This short idle sample does not reveal the preboot
temperature or why the firmware stalled.

## Monitored validation of the warning policy

With the owner at the Dell, one checked C restart was scheduled from boot
`30913da8-5441-41d6-967f-020680316816`. Pinned SSH returned on fresh boot
`9668135f-ca95-4be3-a0d9-78a6fc9a78a7`; the owner saw no warning or unusual
behavior before the desktop. This confirms one normal startup with
`WarningsAndErr=ContWrn`, not the cause of the earlier logo stall or a measured
unattended reliability rate.

The new boot reported healthy SSH, watch and boot status, the same sealed Dell
release, and a UID-1000 Xfce session. A screen capture showed the painted
1920×1080 workspace and dock. The C `power-check`, SSD bootstrap verification,
NV startup marker and protected manifest passed. Firmware had again appended
auto-created USB NIC entries after the two SSD entries; after checking their
identities, `BootOrder` was restored to `0005,0000`. `BootCurrent` remained
`0005`, `DriverOrder` `0000,0001`, and `BootNext` was absent. No second restart
was attempted. The physical-recovery guard remains required for Dell power
commands until a separately tested out-of-band reset path exists.

## Continued VM work

The later terminal source now shows `[scrollback N/512]` in the window title
while viewing history and restores the normal title at the live prompt. Real
X11 testing in `CompanionDev` showed `Heurism C Terminal [scrollback 21/512]`
after 700 output lines and Shift+PageUp, then `Heurism C Terminal` after
Shift+PageDown. The VM release
`/opt/heurism/native/releases/heurism-os-20261009T160434Z-17598` is active,
verified and healthy. This later improvement has **not** been installed on the
Dell. Temporary VM build packages were removed after verification.

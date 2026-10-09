# Dell C terminal update and reboot status, 2026-10-09

This record separates a successful live release activation from an unverified
subsequent reboot. Do not describe the Dell as healthy on the new boot until a
fresh boot ID, management services, desktop health and protected files are
checked through its pinned SSH identity.

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

No further power, firmware or boot-order operation was attempted. The current
boot ID, installed release health and protected-file hashes after the reboot
are **unverified**. Closed-lid `s2idle` wake is previously proven, but it did
not restore access in this attempt and never established wake from full
poweroff or a firmware halt. The existing SSD rescue and root management paths
were not deliberately changed. The next trustworthy check requires local
power/display observation or the Dell returning to its pinned SSH identity.

## Continued VM work

The later terminal source now shows `[scrollback N/512]` in the window title
while viewing history and restores the normal title at the live prompt. Real
X11 testing in `CompanionDev` showed `Heurism C Terminal [scrollback 21/512]`
after 700 output lines and Shift+PageUp, then `Heurism C Terminal` after
Shift+PageDown. The VM release
`/opt/heurism/native/releases/heurism-os-20261009T160434Z-17598` is active,
verified and healthy. This later improvement has **not** been installed on the
Dell. Temporary VM build packages were removed after verification.

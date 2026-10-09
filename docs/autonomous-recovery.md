# Autonomous recovery on the Dell

The internal EFI partition now contains an independent offline Alpine rescue
system, the stable management kernel and initramfs, and a standalone GRUB loader.
The rescue system runs from RAM and loads its kernel modules and signed packages
from EFI. It does not mount the main ext4 installation automatically. Ethernet
DHCP, key-only root SSH and the existing pinned host identity work in both systems.
The SSH client private key remains on this Windows computer.

## Boot behavior

Both the existing Companion Management EFI path and the firmware's standard
`EFI/boot/bootx64.efi` fallback load our standalone recovery configuration.
The menu defaults to the stable management installation. Before launching it,
GRUB writes `companion_pending=1` to the EFI journal. The installed boot-health
service clears that journal only after confirming the installed root, valid SSH
configuration, authorized public key, running listener and network watcher,
and an IPv4 lease. An uncleared journal selects rescue on the next boot.
Missing main kernel/initramfs files trigger immediate GRUB fallback to rescue.
The independent [boot deadline](access-progress.md) now also forces a reset to
rescue if a running installed boot remains unconfirmed for 120 seconds. This
was physically verified with a deliberately invalid main SSH configuration.

Stable kernel and initramfs copies reside at `/boot/efi/companion/stable/`.
The rescue kernel, modloop and package cache are separate from those copies.
Kernel upgrades must update matching stable boot files and preserve the required
root-side kernel modules; do not blindly replace this known working checkpoint.

Kernel panic recovery is configured for fifteen seconds. NMI watchdog detection
is enabled, and soft/hard CPU lockup detection is configured to panic. The journal
handles an unsuccessful previous boot after a reset; it cannot itself reset a
machine stalled before the kernel's recovery facilities work. Physical firmware halts,
total EFI corruption, lost power and failed Ethernet remain outside SSH control.

## Build and verification

```powershell
python tools/build-rescue.py
python tools/stage-recovery.py
python tools/make-fat-image.py build/recovery/payload build/recovery/rescue-test.img
python tests/recovery_smoke.py
```

The builder uses the verified extracted Alpine ISO, its signed APK index and
signed packages. It includes both direct dependencies and APK `install_if`
subpackages. The staged payload is about 392 MiB; the 512 MiB EFI partition also
holds the stable boot files and loaders, with about 86 MiB free.
Staging checks the authenticated host, SSD serial, EFI device, space and transfer
hash. It preserves the original EFI loaders and boot entries under
`/var/lib/companion/efi-backup/`, and does not activate boot changes.
Files retrieved for virtual testing are excluded from subsequent staging archives.

Virtual testing passed offline RAM rescue with trusted root SSH, then forced a
main boot attempt with missing main boot files and verified automatic rescue
fallback after reboot. The test has no main root disk.

Physical internal rescue boot returned authenticated root access with boot ID
`3162e66a-23f5-4b37-af54-0858d5134af1`, RAM root and no mounted main installation.
The USB remained removed. Returning through the new loader to stable management
produced boot ID `99ba4f8e-f20a-4e82-99d0-2c56efebd7de`, the installed ext4 root,
all management services and a committed healthy journal.

The Intel TCO device reported active with a 120-second timeout, but a deliberate
SIGSTOP of its feeder caused no reset within 240 seconds; its reported time-left
remained 120. Trying the driver's SMI-clearing override failed because the
platform did not expose the required SMI I/O resource. The original driver was
restored and the watchdog service disabled on main and rescue. Device presence
and an "active" status are not evidence of a working hardware reset. No manual
intervention was needed to recover this failed test; SSH remained available.

After deliberately marking the previous boot unconfirmed, a real SysRq kernel
panic was triggered on the installed Dell. Its fifteen-second panic recovery
reset the machine and the journal selected internal rescue. Trusted root SSH
returned with boot ID `e5de2887-2ed4-4950-8aa3-a4cb903224f9`, tmpfs root, the main
installation unmounted and the unproven hardware watchdog inactive. Evidence is
saved at `artifacts/hardware/dell-panic-rescue-proof.txt`. No physical assistance
was requested. This proves panic reset and rescue selection; a total hardware
freeze and deliberate CPU-lockup detection have not been tested.

The Dell was then returned to stable management, boot ID
`d69163a3-bdf3-4973-b6fd-aa830f344da7`, with ext4 root, root SSH, the watcher,
the boot-health service, all five crash/lockup settings and a committed journal.

## Select rescue remotely

From the installed management system:

The `companion-next-boot` command selects either system remotely and optionally
reboots. For example:

```powershell
python tools/dell.py --command 'companion-next-boot rescue --reboot'
python tools/dell.py --wait 180 --command 'companion-status'
```

Use `companion-next-boot stable --reboot` to return. Equivalent manual commands:

```sh
grub-editenv /boot/efi/companion/recovery/grubenv set companion_pending=1
sync
reboot
```

Reconnect with `python tools/dell.py --wait 180`. In rescue, EFI is initially
mounted read-only at `/media/nvme0n1p1`. To select the stable installation again:

```sh
mount -o remount,rw /media/nvme0n1p1
grub-editenv /media/nvme0n1p1/companion/recovery/grubenv set companion_pending=0
sync
reboot
```

Rescue keeps selecting itself until that journal is deliberately cleared. Main
root repairs require explicitly mounting `/dev/nvme0n1p2`; rescue does not erase
or reinstall disks on startup. Original EFI loader backups can be restored from
the installed root. Motherboard firmware has not been flashed or replaced.

Sources: [Linux hardware watchdog API](https://docs.kernel.org/watchdog/watchdog-api.html),
[GRUB automatic fallback](https://www.gnu.org/software/grub/manual/grub/html_node/fallback.html).

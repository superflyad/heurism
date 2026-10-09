# Physical Dell deployment — 2026-09-26

The owner explicitly authorized completely erasing the 512 GB SSD, replacing
Windows/files/recovery and installing the persistent Linux management environment.
The 32 GB Optane device was not selected or erased. Motherboard firmware was not
flashed.

## Installation completed

- Target identity checked against Intel H10 serial `PHTE040301J9512B-1`, exact
  size 512,110,190,592 bytes, NVMe transport and AC power before any erase.
- Removed the selected SSD's old Intel RAID and partition-table signatures.
- Created a 512 MiB FAT EFI partition and an ext4 root occupying the rest.
- Installed Alpine Linux with Companion commands, authorized root SSH identity,
  Ethernet/Wi-Fi support and supervised network/SSH recovery.
- Added Chrony for clock synchronization and ethtool for the actual USB Ethernet
  adapter. Network time synchronization was observed in the live environment.
- Installed kernel 6.18.53-0-lts; inspected its initramfs and confirmed both NVMe
  and VMD drivers. GRUB uses `modules=sd-mod,usb-storage,ext4,nvme,vmd`,
  `console=tty1` and the root filesystem UUID below.
- Verified installed SSH host key and authorized client key match the trusted
  live identities; verified Companion and its watcher are enabled at startup.
- Registered UEFI entry 0005, **Companion Management**, first in BootOrder, with
  `\EFI\alpine\grubx64.efi`; standard `\EFI\boot\bootx64.efi` fallback also exists.
- Installation exited successfully. Log and previous partition/boot inventory
  are under ignored `artifacts/hardware/` on the development computer.

Root UUID: `0db596bc-58b0-4280-861c-ddc83c75ae06`.
EFI partition UUID: `23416d37-f41a-4cf2-975c-8225571d1a28`.
Live boot ID before installation/reboot: `f151a64e-1724-4f26-a1d6-8dbfb9f1e3fc`.

## Reboot verification

The first physical reboot with the flash drive removed passed: trusted root SSH
returned at 10.8.22.238, root was `/dev/nvme0n1p2` ext4, kernel was
6.18.53-0-lts, no USB storage was attached, and the recovery service and SSH were
running. BootCurrent was 0005. Boot ID changed to
`d4a185de-6d19-4e9d-87bc-6176e56c9170`. Proof is saved in
`artifacts/hardware/dell-first-installed-boot.txt`.

That boot used one-time BootNext selection. BootOrder was then set to
0005,0000,0003,0004, placing Companion and the firmware-discovered SSD fallback
ahead of PXE. The subsequent normal reboot stopped at Dell's low-wattage adapter
warning. The owner's photo established this was a pre-boot power warning rather
than a loader failure. After Continue, the default SSD entry booted successfully
and root SSH returned with boot ID `16c0acae-433d-4d29-ab2b-6da04240258e`.

## Supported BIOS configuration

The physical Dell exposes BIOS attributes through Linux's `dell-wmi-sysman`
driver. Its Admin and System password indicators reported no passwords set.
Existing values and allowed alternatives were read before writing. Changes were
accepted through that supported interface:

| Setting | Previous | Configured |
| --- | --- | --- |
| PowerWarn (adapter warnings) | Enabled | Disabled |
| WarningsAndErr | PromptWrnErr | ContWrn (continue on warnings) |
| DockWarningsEnMsg | Enabled | Disabled |
| WakeOnAc | Disabled | Enabled |

WakeOnDock and PowerOnLidOpen were already enabled and were not changed. Errors
can still halt startup. These are BIOS configuration changes, not firmware-image
replacement. The previous settings are backed up on the Dell under
`/var/lib/companion/` and on the host at
`artifacts/hardware/dell-bios-settings-before-20260926.txt`.

The following normal reboot returned trusted root SSH without further instructed
console intervention. Boot ID changed to `8c7c3acd-b20c-49a1-b48c-c02547cb0061`;
root remained the installed ext4 SSD. All four BIOS values persisted, and both
the watcher and SSH were running after default-runlevel startup completed.
An initial probe arrived before the watcher had started; a subsequent service
check confirmed startup, rather than treating early SSH availability as proof
that every startup service was already ready.
Linux panic recovery is configured with `kernel.panic=15` and
`kernel.panic_on_oops=1` under `/etc/sysctl.d/90-companion-recovery.conf`.
A later deliberate kernel panic successfully reset into internal rescue; see
the [autonomous recovery record](autonomous-recovery.md).

Sources: [Dell 7506 pre-boot behavior and power options](https://www.dell.com/support/manuals/en-sa/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-silver-service-manual/system-setup-options?guid=guid-cb19996e-6cf5-47b9-be58-1a039da03b99&lang=en-us),
[Linux Dell WMI settings interface](https://kernel.googlesource.com/pub/scm/linux/kernel/git/stable/linux-stable.git/+/7603d8e78023e5883e075b4625fbdf059c6384f7/drivers/platform/x86/dell/dell-wmi-sysman/biosattr-interface.c).

## Access recovery tests

Before installation, the actual Dell recovered automatically after stopping its
SSH listener and after terminating its DHCP client. The complete QEMU installation
test passed actual keyboard login, DHCP and SSH recovery on both USB boot and
installed boot without virtual USB. A local reconnect tool preserves host-key
verification while waiting for startup or discovering a changed DHCP address.

The installed physical system also passed both failures after the final reboot:
its DHCP process was terminated and replaced, its SSH listener was stopped and
automatically restarted, and a fresh trusted root connection then succeeded.
The same boot ID and installed ext4 root were retained. Proofs are saved in
`artifacts/hardware/dell-installed-access-recovery.txt` and
`artifacts/hardware/dell-final-control-verification.txt`.

For commands, reboot recovery and Wake-on-LAN limitations, see
[remote control](remote-control.md). Internal rescue and crash recovery are now
verified in the [autonomous recovery record](autonomous-recovery.md); remote
BIOS-screen interaction has not been established.

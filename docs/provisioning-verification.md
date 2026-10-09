# Provisioning verification — 2026-09-26

Base: official Alpine Linux 3.24.2 extended x86_64. ISO SHA256 was verified against
the checksum from Alpine's download server:

```text
A22115A46A1BCD876234A6CD43797FE19F4ACFC4512FB41EA64E16CC2FFB4D90
```

Custom configuration overlay SHA256:

```text
40A78E6C6B5D95303CFA62DDA0B4448ED84040D33B105DF4259604A15FC9CA40
```

Payload manifest SHA256:

```text
F5CFB80222C99D8FBA0F91E1144AC0CFC3B7A7756B7EC74CE3B6D7256ABAA1A5
```

## Passed checks

- Created a 2 GiB MBR/FAT32 image containing the assembled provisioning payload.
- Booted it as a virtual USB mass-storage device through QEMU/EDK II UEFI.
- Packages loaded from the offline USB repository; root SSH accepted only the
  expected client key and presented the pre-established target host identity.
- Ran the hardware audit and verified POSIX shell syntax inside the Linux guest.
- Previewed the blank virtual target without writing it; refused to erase the
  boot USB when given explicit erase arguments.
- Installed onto a newly created, isolated 12 GiB virtual disk.
- Registered Companion Management first in the virtual firmware's BootOrder.
- Booted that installed ext4 root without attaching the USB, and recovered
  authenticated root SSH and the Companion management commands automatically.
- Confirmed the custom overlay excludes this workspace's client private key;
  authorized key file/directory modes are 0600/0700.
- PowerShell scripts parsed and Python modules compiled successfully.
- Typed through QEMU's emulated keyboard into the standard tty1 getty, logged
  in as root without a password and created a root-owned marker from the local
  shell. This passed for both USB boot and installed boot without the USB.
- Added a supervised access-recovery service; deliberately terminated DHCP and
  stopped SSH and verified automatic recovery on both virtual boot paths.

The final passing run used `python tests/provisioning_smoke.py --install` with
no manual intervention. During development a persistent-startup failure was
found and corrected by explicitly provisioning the management service and SSH
identity on the installed root. The final run re-tested from a fresh virtual disk.

Logs live under `build/provisioning/smoke/`: `checks.txt`, `install.txt` and
`installed-boot.txt`. VM disk images/logs and generated SSH identities are ignored
by Git. Real development-host NVMe disks were not used by the installer test.

## USB publication

Published to the identified 16 GB hp v125w USB (serial 002215D23AE1AC3132CF0019)
on F: without reformatting. All 508 payload files were verified by SHA256.
Changed existing files were backed up under `Companion-backup/` on the USB and
`artifacts/usb-backup/` on the host. The final preparation record is under
`provisioning-20260926-113036/preparation.json` in those backup directories.

A rebuild bookkeeping issue was corrected before completing publication: the
manifest now excludes its own filename, avoiding a stale self-checksum. The
original overlay hash was unchanged from the first passing installation test; the disk image was
regenerated with the corrected manifest. Existing Ubuntu data remains on the USB;
the default EFI loader now starts Companion provisioning.

## Physical console regression and correction

The first physical provisioning boot exposed a test gap: `console=tty0` caused
Alpine to add a tty0 getty alongside its default tty1 getty. Both consumed input
on the same screen, producing repeated password prompts and timeouts despite an
empty root password. Ctrl+Alt+F2 provided a separate terminal and let the owner
log in. SSH-only testing had missed the competing local login processes.

Both USB GRUB and the internal installer's kernel options now use `console=tty1`.
The complete virtual install/reboot test passed again, including actual keyboard
login on both boots. The identified physical USB was patched over SSH, and each
of the three changed files matched its staged copy and expected SHA256. Old
copies are under `Companion-backup/console-fix-20260926/` on the USB and
`artifacts/usb-backup/console-fix-20260926/` on the host. The running installer was
updated and the tty0 line removed from its RAM configuration. At that stage no
reboot or internal disk write had been performed. Subsequent internal installation
and physical SSD boot are recorded separately in [Dell deployment](dell-deployment.md).

## Physical target verification and limitations

Dell provisioning boot, local root login on a separate terminal, Ethernet and
host-key-verified root SSH were confirmed on the physical device. The hardware
audit was collected to `artifacts/hardware/dell-7506-20260926.txt`. After explicit
owner authorization, the 512 GB SSD was erased and installed; physical boot from
its ext4 root without USB restored trusted root SSH. Wi-Fi association remains
untested. No Dell motherboard firmware was flashed.
The provisioning and installed management system use Linux; the native Companion
kernel, native drivers and firmware replacement remain separate work.

## Access-hardening regression build

The later access-hardening payload added the pinned public identities, stronger
SSH listener checks and optional installed boot-health/deadline services. The
complete isolated QEMU install test passed again: UEFI USB root SSH, keyboard
login, hardware audit, erase guards, DHCP/SSH recovery, installation to a fresh
virtual disk, and installed boot without the virtual USB, including keyboard
login and access recovery. Test output is under `build/provisioning/smoke/`.
This is a rebuilt local test image; the owner's removed physical USB was not
rewritten during this work.

The tested overlay SHA256 was
`13D4C3CD2966FCB27D4EF5593F4F2D411828DE31A1D99055FC2A2B11A51E50AF`;
its manifest SHA256 was
`26BA010F3C778577B540D7612347DEA118ABC32797CF0146A7E538860FE6796C`.
Subsequent deadline housekeeping marks a healthy one-shot guard complete rather
than leaving its completed process reported as crashed; it was checked on Dell.

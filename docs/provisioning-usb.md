# Companion provisioning USB

This is the first installation/management environment for target 0. It boots a
custom configuration built on Alpine Linux 3.24.2, starts key-authenticated root
SSH, provides Wi-Fi/Ethernet setup, audits hardware and can install that management
environment permanently onto an explicitly selected internal disk.

It is temporary Linux infrastructure for developing Companion. It does not claim
to contain our future native kernel, replace the Dell BIOS or unlock Boot Guard.
The original Companion UEFI framebuffer experiment remains a second boot option.

## Boot and connect

1. Keep Secure Boot disabled for the development image.
2. Connect the Dell to the same router using Ethernet if available. Otherwise
   use the Wi-Fi command below. Boot the USB's UEFI entry from F12.
3. Choose **Companion provisioning - remote access and installer** (the default).
   Packages load from the USB into RAM; allow the first startup to finish.
4. Log in locally as `root`; the local provisioning console has no password.
   Remote password login is disabled. Run `companion-status` for the IP address.
   If the initially published USB shows repeated login/password prompts, press
   **Ctrl+Alt+F2** and log in as `root` on that separate terminal. The first image
   mistakenly started gettys on both `tty0` and `tty1`; these compete for the same
   display and keyboard. Rebuilt images use `console=tty1` to avoid that duplicate.
5. For Wi-Fi, run `companion-wifi` and enter the SSID and password **on the Dell**.
   It supports WPA-PSK home networks. Credentials stay in RAM for the USB session;
   an internal installation preserves the configuration.
6. Send this workspace the address shown by `companion-status`. The host can use:

```powershell
.\tools\connect-dell.ps1 -Address 192.168.1.123
.\tools\connect-dell.ps1 -Address 192.168.1.123 -Command 'companion-audit; cat /var/lib/companion/audit.txt'
```

Use the actual address, not the example. Same router still requires devices to be
able to communicate; guest-network/client isolation can prevent the connection.

SSH verifies the target's generated host identity. The authorized client private
key is only on this development computer under `artifacts/ssh/`; the USB contains
its public key and the target's own host key. Keys and hardware reports are ignored
by Git. Keep this USB as personal provisioning media, not a public distributable.
This creates command access while the target OS/network is running, not a channel
through power-off, arbitrary BIOS screens or kernel crashes.

`companion-watch` supervises recovery of DHCP clients and the SSH listener after
startup. Use `python tools/dell.py --wait 180 --command 'companion-status'` to
reconnect after a reboot or DHCP address change. It requires the pinned SSH
identity before using an address discovered on the configured local /24. Commands
are executed once, never retried automatically. See [remote control](remote-control.md).

## Permanent internal installation

Boot alone does not change internal disks. First obtain the actual disk inventory:

```sh
companion-install
companion-install /dev/nvme0n1
```

The second command is a preview. The device name above is an example. Only after
choosing the exact disk whose entire contents may be replaced, use:

```sh
companion-install /dev/nvme0n1 --erase /dev/nvme0n1
```

That command erases **all partitions on that disk**, including Windows/data/recovery
partitions, and installs an ext4 Alpine-based system with an EFI boot partition.
It carries forward SSH identity, Wi-Fi configuration and management tools, registers
**Companion Management** as a preferred UEFI startup entry when firmware accepts
the change, and provides the standard removable EFI fallback path. It refuses USB
targets, partitions instead of disks, and disks with mounted filesystems. No
motherboard firmware flash command is performed.

An installation is not a disk-data backup. The audit is read-only hardware
information and the boot-variable record is not a complete BIOS image backup.
Firmware replacement still requires a supported board-specific implementation,
protection-state investigation and a verified recovery method.

## Rebuild and test

```powershell
.\tools\build-provisioning.ps1
python .\tests\provisioning_smoke.py --install
```

Dependencies: Python, OpenSSH client/key generator, 7-Zip for official ISO
extraction, and QEMU with bundled EDK II firmware for the smoke test. The builder
pins Alpine 3.24.2 and checks the ISO against its published SHA256. Packages are
loaded through Alpine's offline signed package repository. The custom overlay and
all payload files have a generated manifest. Private client keys never enter it.

The builder generates `build/provisioning/payload/` and a 2 GiB MBR/FAT32 image
`build/provisioning/companion-usb.img`. The test image is an ordinary file and
the install test uses a newly created, isolated 12 GiB virtual disk. It verifies
UEFI USB boot, root SSH with the expected host key, hardware audit, safe disk preview,
boot-USB refusal, installation and persistent access after removing virtual USB.
Both boot paths test actual local keyboard login, forced DHCP-client failure and
automatic recovery after stopping the SSH listener. The installed kernel and
additional packages may be newer than the pinned live ISO when online repositories
are available; record the resulting package versions for physical deployments.

To update a prepared FAT32 USB without reformatting, specify its drive and exact
serial number:

```powershell
.\tools\prepare-provisioning-usb.ps1 -DriveLetter F -ExpectedSerial '<the USB serial>'
```

The wrapper validates that it is a non-system USB before copying. Changed files
are backed up on USB and host, and each copied file is verified by SHA256. Existing
Ubuntu data may remain but the default boot path becomes Companion provisioning.

Sources: [Alpine downloads](https://alpinelinux.org/downloads/),
[OpenSSH manuals](https://www.openssh.org/manual.html),
[Dell startup configuration](https://www.dell.com/support/manuals/en-us/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-black-service-manual/system-setup-options?guid=guid-cb19996e-6cf5-47b9-be58-1a039da03b99&lang=en-us).

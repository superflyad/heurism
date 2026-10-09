# Network-delivered RAM management: implemented, physical boot unproven

The management image contains the target's matching Linux kernel, a complete
RAM root, USB Ethernet drivers, DHCP, key-only root SSH and the pinned target
host identity. GRUB embeds both kernel and initramfs into one x64 UEFI
application. Its configuration does not search for a disk or read boot files
from an ESP. Storage modules are excluded and storage drivers are blacklisted.
This is Linux scaffolding for Companion management, not a new Companion kernel
or replacement Dell firmware.

## Verified results

The complete firmware PXE → TFTP → standalone GRUB → kernel → RAM root path
passed in QEMU with Debian OVMF and **no disk device configured**. Root SSH
authenticated against the existing pinned host identity. Root mounted as
`rootfs`, no `/dev/` filesystem was mounted and no SSD block device existed.
The VM also recovered its SSH listener and DHCP client after both were killed;
the boot ID stayed unchanged. This test does not establish a working firmware
reset path on the physical Dell.

Latest VM boot: `49d54fc0-634a-4566-9721-f560460e5a1d`.
Evidence: ignored `build/network-root/test/pxe-proof.json`, `pxe-proof.txt` and
`serial.log`. EFI size: 38,277,120 bytes. SHA256:
`34993c31dc2273d8e7a397ca9bbd7c95e49b5c2ebfd4aca0ab5ae21f89b7b2d0`.
The official Debian package-index hash was checked before extracting OVMF;
its source record is `build/network-root/ovmf/source.json`.

The Dell's running SSD system fetched the small probe through the temporary
LAN TFTP server and verified its complete SHA256 before reboot. The probe is
29,184 bytes, SHA256
`b5229a6ec84ab72d765cae88b01865a6e1c98fd85a53b8d5227933a0ac11dffc`.
The probe now returns directly to Boot Manager when loaded from a network
handle; it never appends a disk path to that network handle and recursively
reloads the PXE image.

## Physical test and current limitation

The physical test selected the existing USB NIC IPv4 entry `0002` using
BootNext. BootOrder stayed `0005,0000`. DriverOrder was temporarily narrowed
to `0000`, omitting the SSD Companion driver so it could not supply the
extension during the network test. Existing SSD boot/recovery files were
unchanged. Pre-test healthy root boot was
`a7f66b32-1a47-4a47-b5ac-27407ce9efbe`.

On 2026-09-27 UTC the server saw firmware DHCP discoveries at 03:28:42,
03:28:46, 03:28:54 and 03:29:10, transaction `2429762340`. There was no DHCP
ACK, TFTP download or native probe execution. The first server used a subnet
broadcast destination; it was corrected to the limited broadcast address and
client-identifier echo was added, but neither correction has been verified
with the Dell's firmware. Ordinary Linux DHCP clients are ignored.

Two bounded reconnection attempts, including Wake-on-LAN, did not return the
pinned root connection. A subsequent owner photo on 2026-09-27 confirmed Dell
SupportAssist stopped at **"No bootable devices found"**, with a Continue
button. The preserved SSD BootOrder therefore did not automatically recover
this native PXE failure. The full RAM image was not attempted on the
physical laptop. This experiment has **not maintained independent control**
through this firmware boot failure; Ethernet SSH and a wake packet cannot
clear a firmware prompt or reset a running machine. SSD fallback files alone
do not guarantee that Dell Boot Manager will execute them after a PXE error.

No further physical boot changes should be attempted until pinned root access
returns. The laptop is stopped in firmware, so an owner restart/Continue is
needed to resume the existing SSD boot. BootNext is normally consumed on use;
its post-test absence must still be verified. Restore DriverOrder to
`0000,0001` after authenticating to the installed management system. The
temporary PXE service is stopped when this work session is cleaned up.

## Build and isolated verification

```powershell
python tools/build-network-root.py
python tools/build-network-efi.py
python tools/fetch-pxe-ovmf.py
python tests/network_ram_smoke.py --uefi-pxe --exercise-recovery
& 'C:/Program Files/Adobe/Adobe Dreamweaver 2021/node/node.exe' tests/network_boot_server.cjs
```

Builds read the matching kernel/modules through existing authenticated SSH and
stage build inputs under `/var/lib/companion/network-root-build/`; they do not
change installed boot files. RAM startup has a 120-second readiness reboot
guard. This begins after Linux init starts; it cannot recover a firmware hang.
The watcher restarts missing DHCP clients and a terminated SSH listener. It
does not yet test or recover every type of frozen process or hardware failure.

The physical launcher `tools/start-network-boot.py` is now blocked, and the SSH
helper rejects native PXE selection of entries 0001/0002/0003. Future physical
network experiments must run from the proven Companion SSD loader and transfer
explicitly to its normal management loader on failure. This preserves the SSD
dependency; it is not SSD-independent startup. See `AGENTS.md`.

## Access restored after the owner cleared the halt

On 2026-09-27 pinned root SSH returned at `10.8.22.238`, boot ID
`1ba912c9-289e-4c0d-aa8c-03b18f40b52a`. BootCurrent was `0005`, the installed
SSD management system. Firmware had added a duplicate IPv4 NIC entry `0001`
and changed BootOrder to `0005,0000,0001,0003`. DriverOrder still showed `0000`.
Both orders were restored: BootOrder `0005,0000`, DriverOrder `0000,0001`.
There was no pending BootNext. No reboot was performed during restoration.

The main, fallback and recovery EFI loaders are present and all hash to
`5bc0e512af43def3ad39ce90b5084f1e56c73abc98d520e01466b7a7c7724efc`.
Stable kernel/initramfs and recovery configuration exist. The recovery journal
reports `companion_pending=0`; SSH and the network watcher are running and the
boot deadline reports the current boot confirmed healthy. The guard now also
covers the new NIC entry, including NIC entries later in BootOrder, while
allowing DriverOrder restoration in its separate namespace. Guard tests pass.

The original server implementation starts a hidden, 30-minute temporary
service using an installed Node runtime's existing Domain/Private LAN rule.
It binds DHCP 67, TFTP 69, readiness HTTP 18080 and owner UDP 18081. DHCP is
restricted to MAC `7c:c2:c6:1d:b2:f5` and PXEClient vendor class, address
`10.8.22.238`; requests selecting another DHCP server/address or using a relay
are ignored. TFTP serves one exact filename and limits peers to the target and
controller. No general-purpose file directory is exposed. Unit tests verify
these DHCP exclusions, malformed-packet handling and client-ID echo.

The image includes the target's SSH host private key and must remain in ignored
build storage on the owner's trusted LAN. The controller's client private key
is never embedded. TFTP is an unauthenticated transport; a server-side hash
check is not cryptographic verification by the firmware. Signed/authenticated
boot delivery remains future work before expanding the trust boundary.

Sources: [GRUB memory-disk device syntax](https://www.gnu.org/software/grub/manual/grub/html_node/Device-syntax.html),
[EDK II PXE offer selection](https://github.com/tianocore/edk2/blob/master/NetworkPkg/UefiPxeBcDxe/PxeBcDhcp4.c),
[DHCP client identifier echo](https://www.rfc-editor.org/rfc/rfc6842.html).

# Physical HTTP boot prerequisite inspection

On 2026-09-27, the read-only SSD UEFI probe completed and trusted root access
returned on boot `dbb8c399-d20b-4ee7-8218-d0512098a1bd`. This checks the actual
firmware interfaces before considering driver initialization. It does not
execute HttpBootDxe, invoke its vendor policy method, attach a NIC, dispatch
firmware drivers, or request a network boot.

| Check | Physical result |
| --- | --- |
| HTTP boot PE section | Read through FV2, 58,464 bytes; full byte comparison matches the pinned saved image |
| HTTP boot dependency section | Read through FV2, 219 bytes; full byte comparison matches the pinned SOR dependency expression |
| DellBoardPolicyDxe PE section | Read through FV2, 3,968 bytes; full byte comparison matches the saved provider |
| Ethernet controller | One HTTP service binding controller; device path matches MAC `7c:c2:c6:1d:b2:f5`; DHCP4 and DHCP6 service bindings on that same handle |
| Live policy interface | Found; lies inside the loaded DellBoardPolicyDxe image at RVA `0xc38`; first method belongs to that image at RVA `0x894`, as in the saved provider |

Fifteen FV2 interfaces were inspected. HTTP sections were found in volume index
8, the provider in index 3. ReadSection returned success and authentication
metadata zero for each matching section. Zero is recorded as returned metadata,
not treated as a signature or proof of Secure Boot authentication. The saved
pins are listed in [the initialization research](http-driver-initialization.md).

The live policy provider identity is established through its FV-file device
path and image ownership of the interface/method. This does not verify every
relocated runtime byte, inspect the provider's current callback registration
lists, or establish its response to the HTTP driver's policy request.

## Management preservation

Boot/recovery loader hashes, the NV startup loader, root health and services
were checked before reboot. The owner probe entry `0004` was selected once
through BootNext; the default SSD order remained `0005,0000`. The report reached
`HTTP_PREREQUISITES_COMPLETE` and `PROBE_COMPLETE` before the normal management
handoff. BootCurrent after return was `0000`, the verified SSD fallback loader.
This confirms report completion and healthy management return; it does not
identify every step inside the GRUB/firmware handoff.

Dell appended its USB NIC options behind the default SSD entries during the
reboot. Their device paths were checked, the observed order archived, and the
default `0005,0000` restored. Temporary entry `0004` was removed. DriverOrder
remains `0000,0001`, BootNext is absent, and the owner NV payload is unchanged.
The post-test boot-variable backup verifies all three normal/recovery EFI files.

The probe and management startup still require the SSD. Nothing in this result
establishes SSD-independent startup or an independent reset/control channel.

## Reproduction and evidence

`tools/build-firmware-probe.ps1 -HttpPrereqProbe` generates comparison arrays
from hash-pinned research files, builds the freestanding x64 EFI application,
and runs host fixtures for buffer/status checks, paths, protocol absence,
interface ownership and range limits. The full inventory fixture leaves
execution/attachment callbacks unset. Common fixtures exercise report writes
and the normal SSD GRUB handoff. All passed.

`tools/run-firmware-probe.py --http-prereq-probe` stages without rebooting.
`tools/verify-http-prereq.py reboot` verifies the installed recovery paths before
selecting the owner entry once. Its `verify` action requires a new healthy boot,
collects the completed report, checks the preserved loaders/driver order, then
removes the temporary entry and restores the default order. A completed report
is retained; staging refuses to overwrite it.

Evidence under ignored `artifacts/firmware/`:

- `http-prereq-observation.txt`, SHA256 `de31eb9341aeff112f1a9befbe9ec4d55c826136eeabedd14da1ee5b1cf4b07e`.
- `http-prereq-probe-deployment.json` and before/staged/after boot-entry reports.
- `boot-variable-backups/20260927T143040Z/snapshot.json` before deployment and
  `boot-variable-backups/20260927T143130Z/snapshot.json` after recovery/cleanup.

The physical probe image SHA256 is
`e7985226a0a096ac2e17e5fa30ced06d576ed8c4ac5ee666e5d975352a5a50e4`.

The prerequisites now support evaluating targeted driver loading/registration
as a separate experiment. Controller attachment and a bounded network transfer
remain subsequent unproven steps. Global dispatch and native PXE selection are
not part of this path.

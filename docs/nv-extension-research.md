# Board-stored Companion payload experiment

The goal is to retain Dell/Intel firmware while placing Companion deeper than
an SSD-only installation. Research identified a bounded route through standard
UEFI variable services: store the existing owner driver image as one small
nonvolatile variable and explicitly load the bytes retrieved from that variable.
This is not a new driver embedded into a Dell firmware volume. A bootstrap is
still needed; the current test bootstrap resides on the SSD.

## Implemented experiment

`boot/nv_extension_probe.c` stores only the exact previously hardware-tested
2,048-byte Companion UEFI boot-service driver. The payload SHA-256 is
`b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a`.
There is no arbitrary image or flash-address input. The owner variable is:

```text
Name:       CompanionExtensionImage01
Vendor GUID: 1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1
Attributes:  7 (nonvolatile, boot-service access, runtime access)
Data:        the exact 2048-byte EFI driver image
```

The probe first queries capacity, requires more than 64 KiB plus the payload
remaining and an adequate per-variable size, and creates the variable only
when absent. It never overwrites a differing existing image. It retrieves the
stored bytes into a separate buffer and compares every byte with the pinned
reference. Only that retrieved buffer is passed to LoadImage. StartImage runs
the driver; HandleProtocol on the new child image and GetInfo verify that the
newly loaded driver's service responds. The original SSD extension remains
active, but querying the new child prevents it from providing a false positive.

The test bootstrap contains an embedded reference image for comparison and
initial creation; it does not load that reference buffer. Thus the first test
is a write/readback/load test, and the repeat is a persisted-read/load test.
It is not a bootstrap that can run with every SSD file absent.

Host tests exercise initial creation, an existing matching image without a
repeat write, payload and attribute mismatches, inadequate space, failed
writes, failed image loading and failed image startup. They also assert that
the source buffer passed to LoadImage is the retrieved buffer rather than
the embedded reference. The freestanding x64 application has relocations
and no OS imports. Its SHA-256 is
`ea5a5a22f080f476e7d4ed772b624c331aaf3c0d0eb958e828c4f8f7d8c8ebb7`.

## First physical boot and flash observation

On boot `83a3b9b4-9d0a-4318-b0f6-2822b2eddcc2`, native UEFI reported:

| Check | Result |
| --- | --- |
| Maximum variable-store capacity | `0x5d79c` bytes |
| Remaining before creation | `0x383cc` bytes |
| Maximum individual variable | `0xffc4` bytes |
| Owner payload initially present | No; EFI_NOT_FOUND |
| SetVariable and readback | EFI_SUCCESS; 2048 bytes, attributes 7 |
| Exact byte comparison | Passed |
| LoadImage / StartImage from retrieved buffer | Both EFI_SUCCESS |
| Child's Companion protocol / GetInfo | Both EFI_SUCCESS; expected service magic/revision/capability |

Root SSH and the watcher returned healthy after boot. An OS-side read of the
firmware variable returned attributes 7 and the identical payload hash.

An independent hardware READ of the 16 MiB BIOS region has SHA-256
`9d0bda3de694ca2952c6a73575540c09580da639b2de52459cd87b6d53b6b8ad`.
The exact owner image appears at BIOS offset `0x40494`, corresponding to
logical SPI flash offset `0x840494`, inside the known variable-storage area.
The UTF-16 owner variable name is present immediately before its vicinity.
The image is absent from the earlier hardware snapshot, and all BIOS bytes
from offset `0xd0000` onward match that snapshot. This independently establishes
physical storage of the owner image in motherboard flash through the variable
service, without changing the BIOS code outside the variable area.

This route does not contradict the earlier denied direct PROGRAM request:
the firmware's supported variable service performs authorized storage updates;
our ordinary direct SPI backend still has not gained BIOS-volume write access.
Variable data is not automatically treated as a dispatchable firmware module.

## Second physical boot and final configuration

On boot `175c510f-bba0-4902-a461-d2de7404a51d`, the payload was already present
with attributes 7 and the same exact bytes. No SetVariable creation was needed.
LoadImage, StartImage, the new child's protocol and GetInfo all passed again.
The OS-side readback retained the identical payload hash, and root SSH and
the boot-health watcher returned healthy on both boots.

The temporary probe boot entry was removed after verification. Normal BootOrder
is restored to `0005,0000`; DriverOrder remains `0000,0001`, retaining Dell's
setup entry and the original automatic SSD extension. The board-stored payload
remains in NVRAM; the experiment has not made its loader the default boot target.

Evidence is under ignored `artifacts/firmware/`: `nv-extension-verification.json`,
`nv-extension-observation-1.txt`, `nv-extension-observation-2.txt`,
`nv-extension-flash-location.json`, `nv-extension-payload-readback.bin`, and
`bios16-with-nv-extension.json`. The verification JSON records both boots and
the final boot configuration.

## Scope and next gate

The subsequent [automatic dispatch investigation](nv-dispatch-research.md)
tests Dell's saved image-source resolver offline and backs up the live boot
variables. Raw flash addresses and variable GUIDs did not provide an image;
an initial loader or matching image provider remains necessary.

The experiment establishes a small owner-code storage path and explicit native
execution through a bootstrap. It does not establish direct firmware dispatch
from NVRAM, operation with the SSD removed, preboot remote management, SMM
privilege, changes to Boot Guard trust, or a general BIOS-volume flash backend.
The initial experiment left the automatic extension loading its SSD copy.
The subsequent [startup update](nv-startup.md) makes Driver0001 retrieve and
verify the NVRAM payload, with an embedded SSD fallback. A normal physical boot
now proves automatic NVRAM execution; the initial bootstrap still needs the SSD.

The next unresolved gate is a bootstrap source independent of the SSD. UEFI
defines firmware-volume and network-loading interfaces, but their availability
and suitability for this Dell must be tested. Network bootstrap would still
depend on an available server. No network boot claim is made by this test.

The subsequent [preboot network tests](preboot-network-testing.md) establish
native Ethernet transfer and explicit execution from downloaded bytes, plus
safe return to root access with the transfer controller offline. Their initial
bootstrap still starts from the SSD; direct network startup remains unproven.

Build with `tools/build-nv-extension.ps1` and explicit Clang/lld-link paths.
Native deployment is `tools/run-firmware-probe.py --nv-probe --reboot`.
`tools/verify-nv-extension.py verify`, `retest`, and `finalize` capture evidence,
require healthy pinned root access and restore normal boot configuration.
The owner payload remains stored; the temporary test boot entry is removed.
`tools/verify-nv-flash-location.py` reproduces the physical location checks from
the downloaded hardware snapshots. All artifacts preserve source hashes.

Sources: [UEFI variable storage, capacity and persistence](https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html),
[LoadImage from a source buffer](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html),
and [UEFI network-loading interface](https://uefi.org/specs/UEFI/2.11/13_Protocols_Media_Access.html).

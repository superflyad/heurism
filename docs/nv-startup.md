# Automatic startup from the motherboard variable

The current Dell bootstrap also accepts and prefers the Heurism variable.
See [the 2026-10-09 migration](heurism-nv-migration.md) for the live startup
state; the observations below record the original Companion-only rollout.

On 2026-09-27, a normal Dell reboot automatically loaded and executed the
verified Companion extension from its nonvolatile motherboard variable.
Trusted root SSH returned, the management watcher started, and the new boot
was confirmed healthy. No USB or one-time boot application was used for this test.

The boot path is:

```text
Dell UEFI -> existing Driver0001 on SSD -> read/verify NVRAM extension
          -> LoadImage/StartImage -> verify child's owner service
          -> normal Companion management boot -> pinned root SSH
```

The initial driver still resides on the SSD. This establishes automatic
execution of motherboard-stored code, not SSD-independent startup, firmware
replacement or remote control of BIOS error screens. The owner service lasts
until ExitBootServices and currently supplies read-only information, not preboot
remote management.
The subsequent [independent bootstrap investigation](independent-bootstrap.md)
surveys existing Dell network/recovery providers and tests the HTTP URI scheme
check. It identifies a candidate, not a working SSD-independent channel.

## Installed behavior

`boot/nv_startup.c` reads `CompanionExtensionImage01` under GUID
`1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1`. It requires attributes 7, exactly 2048
bytes, and an exact match to the physically verified payload. LoadImage receives
the retrieved variable bytes, not the embedded comparison bytes. After StartImage,
it queries the newly loaded child handle and verifies service revision, size,
magic and capabilities. The owner variable is never created, replaced or deleted
by this startup driver.

Missing, corrupt, oversized, incorrectly attributed or failed images take the
embedded original SSD-service fallback. Both paths return to Dell's normal boot
manager. The code performs no network transactions, changes no boot entries,
and invokes no flash programming or protection operations. Firmware API calls
are not an independent reset channel; there is no claim of recovery from every
possible firmware hang.

The driver publishes a 64-byte diagnostic variable `CompanionNvStartup01` with
attributes 6 (boot-services and runtime access, **not nonvolatile**). Its fields
are magic, version, path, read status, load status, start status, service status
and fallback status. Path 1 means verified NVRAM execution; path 2 means SSD
fallback. This transient evidence is separate from the persistent owner payload.
The verification tool saves it locally with the new healthy boot ID.

The existing Driver0001 path remains `\EFI\companion\companionextx64.efi`.
Its new SHA-256 is
`2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4`.
The original driver remains at
`\EFI\companion\companionextx64.ssd-backup.efi`, with SHA-256
`b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a`.
DriverOrder remains 0000,0001, retaining Dell's setup driver.

## Validation and physical evidence

Thirteen host cases cover valid execution, missing/corrupt/wrong-sized or
wrong-attribute payloads, runtime absence, load/start/service failures, null
child handles and diagnostic-write failure. Three isolated OVMF boots execute
the actual new EFI binary: valid NVRAM succeeds, while absent and corrupt NVRAM
fall back and still expose the original owner service. The fixture application
that writes test variables is VM-only and was not deployed to the Dell.

The physical normal boot changed from
`1ba912c9-289e-4c0d-aa8c-03b18f40b52a` to
`f8fef63e-2976-4c0c-a384-28fe70247786`. Its volatile marker reports path 1 and
EFI_SUCCESS for read, LoadImage, StartImage and child-service verification.
Root SSH and companion-watch are healthy. The payload hash remains unchanged;
all three management/fallback/recovery EFI loaders retain their known hashes.
Physical missing/corrupt-variable tests were not performed.

Dell appended automatic USB NIC entries to BootOrder during the reboot, yielding
0005,0000,0001,0002. Management still booted from 0005. The observed entries were
recorded, then the established SSD order 0005,0000 was restored. No NIC entry was
selected, deleted or made a test target. BootCurrent is 0005, BootNext is absent,
DriverOrder remains 0000,0001, and recovery state is `companion_pending=0`.
The firmware's tendency to append NIC options remains a limitation; this test
does not prove permanent BootOrder immutability.

Evidence: `build/nv-startup/host-verification.json`,
`build/nv-startup/vm-verification.json`,
`artifacts/firmware/nv-startup-deployment.json`,
`artifacts/firmware/nv-startup-firmware-order-after-boot.txt`, and
`artifacts/firmware/boot-variable-backups/20260927T133126Z/snapshot.json`.

## Reproduction and rollback

Build using `tools/build-nv-startup.ps1` with explicit Clang and lld-link paths.
`tests/nv_startup_smoke.py` accepts the same `--clang` and `--linker` paths and
uses the repository's existing isolated QEMU/OVMF runtime.

`tools/run-nv-startup.py deploy` requires passing host and VM results, backs up
variables, checks target identity and recovery files, saves the original driver,
and installs the exact tested binary. It refuses a duplicate deployment.
`reboot` rechecks files and management health before requesting a normal reboot;
it does not set BootNext. Reconnect with `tools/dell.py --after-boot` using the
old boot ID. Wait for the current boot's healthy marker before `verify`.
Unexpected boot/driver order stops the tool for review.

`python tools/run-nv-startup.py rollback` restores the verified original SSD
driver without changing boot/driver registrations or the owner variable. It
does not reboot automatically. This rollback requires working root access;
the saved file is not an out-of-band recovery mechanism.

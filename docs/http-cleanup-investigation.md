# HTTP controller cleanup investigation

The physical NIC relationships and a full saved-code lifecycle replay explain
why calling Stop on the original Ethernet handle can report success without
cleanup. The implementation also ignores several cleanup failures. Physical
HTTP controller attachment has not been attempted.

## Physical relationships

The read-only probe completed on 2026-09-27, boot
`73ece2ba-139c-4c0c-8369-0c795cd39c40`, with trusted root SSH and the management
watcher healthy afterward. It did not load a driver, attach a controller,
disconnect anything or start a network transaction.

| Query on MAC-selected Ethernet controller | Observed result |
| --- | --- |
| MNP protocol open information | EFI_NOT_FOUND |
| DHCP4 protocol open information | EFI_NOT_FOUND |
| DHCP4 service binding | Present, three open records |
| HTTP service binding | Present, one open record |
| SNP | Present, including a BY_DRIVER open |
| HttpBootDxe private caller-ID protocol | EFI_NOT_FOUND |
| MNP protocol handles anywhere | None |
| DHCP4 protocol handles | One existing child |

The existing DHCP4 child had one BY_DRIVER record whose ControllerHandle was
the selected Ethernet NIC. That existing child belongs to the current firmware
stack; the experiment left it untouched. Handle addresses are observations of
this boot, not identifiers to reuse after reboot.

Stop at RVA `0x13e0` searches **MNP/DHCP4 protocols**, not their service bindings.
Its helper `0x2674` reads the supplied handle's open records; `0x9d48` selects a
BY_DRIVER record and returns its ControllerHandle. The absent protocols on the
physical NIC therefore explain the original-NIC fixture's no-op branch. A
future Start-created DHCP child must be identified by its actual ownership
record; the existing firmware child is not a substitute.

## Full lifecycle replay

`tools/test-dell-http-lifecycle.py` executes the pinned module's IPv4 Supported,
full Start and Stop instructions in Unicorn. All internal path, formatting,
HII package setup and HII cleanup routines execute without patches. External
boot services, runtime GetVariable, SNP, DHCP service binding and HII protocol
methods use explicit stateful fixtures. Every unreviewed service or interface
call stops the replay. Each call has instruction and time limits.

The driver binary SHA256 is
`d992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788`.
The entry constructor is covered by the separate four-case entry replay; the
lifecycle fixture supplies its initialized service pointers. Only IPv4 before
LoadFile transfer is exercised. DHCP packets and HTTP requests are not modeled
or transmitted. Firmware-internal disconnect/reconnect during uninstall is
not modeled, and failure results are injected explicit service outcomes.

Nine cases assert these behaviors:

| Fixture | Result after driver returns |
| --- | --- |
| Successful Start, Stop(original NIC) | Stop success; six protocol interfaces, two owned opens and one HII package remain |
| Successful Start, Stop(DHCP child) | Stop success; protocols, opens and HII package removed; private state freed |
| DHCP CreateChild fails | Start error; created driver state cleaned up |
| IP4_CONFIG2 open fails | Start error; DHCP child destroyed and state cleaned up |
| HII AddPackages fails | Start error; partial HII state cleaned up |
| HTTP child protocol removal fails | Stop success; LoadFile points into freed child state |
| HII interface removal fails | Stop success; HII/configuration interfaces point into freed memory |
| DHCP DestroyChild fails | Stop success; DHCP child remains |
| HII RemovePackageList fails | Stop success; HII package remains |

The nominal cleanup leaves one 72-byte HTTP child device-path allocation in
this fixture. The saved destroy routine frees the child state but does not
free that path. Its interfaces are removed, so this is an allocation leak in
the tested path, separate from the dangling-interface failure cases. It would
be reclaimed with boot-service memory at ExitBootServices; this does not make
the failure cases safe.

These tests pass when the expected behavior—including unsafe behavior—is
observed. They are not a safety certification for physical attachment.

## Candidate route and required safeguards

The subsequent [targeted disconnect verification](http-targeted-disconnect-verification.md)
replays the saved Dell DXE dispatcher. Eight cases pass: the nominal owned-child
route works, while four injected vendor cleanup failures still return success
with leftover state. Offline postconditions reject those results. This does
not yet justify physical attachment or prove autonomous failure recovery.

Direct Stop(DHCP child) was used to characterize machine code **offline**. It
does not demonstrate compliance with the direct Stop calling restrictions.
The [EDK II interface documentation](https://github.com/tianocore/edk2/blob/master/NetworkPkg/HttpBootDxe/HttpBootDxe.c)
requires a previous Start controller for direct Stop calls.

The [UEFI DisconnectController service](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html#efi-boot-services-disconnectcontroller)
uses handle-database open records to select managing drivers and invokes their
Stop methods. A specific driver handle limits the request to that driver.
It can also return success when the specified driver was not managing the
handle, so its status alone is not proof of cleanup.

The candidate for further offline verification is therefore:

1. Snapshot existing NIC/child handles and open records before attachment.
2. Identify only the newly created DHCP child with a BY_DRIVER open whose
   AgentHandle is this newly loaded IPv4 driver binding and ControllerHandle
   is the MAC-selected NIC. Preserve existing DHCP/SNP/HTTP service providers.
3. Review/replay the firmware's targeted DisconnectController route for that
   child and binding handle; never call the vendor unload routine or disconnect
   all drivers from the physical NIC.
4. Snapshot the created HTTP child, HII handle/package and private interface
   before Stop can free backing state. Inspect using saved handles and protocol
   queries afterward; never dereference a possibly freed private structure.
5. Require zero owned protocols, open records and HII packages after cleanup.
   Retain driver image memory until ExitBootServices. Do not free the retained
   child path while references might remain.
6. On a failed removal or incomplete cleanup, do not continue to management in
   the same firmware session with dangling interfaces. A default-SSD reset is
   a proposed recovery action, but independent recovery from a firmware halt
   is still unproven. That limitation must be resolved or explicitly bounded
   before authorizing a physical attachment experiment.

An owner-controlled cleanup implementation that checks each operation and
retains backing memory on failure is another candidate. It would need a fully
validated state/ownership snapshot and tests before use; it is not installed.

## Evidence and reproduction

- Build: `tools/build-firmware-probe.ps1 -HttpCleanupProbe`.
- Twenty host cases cover prerequisite checks, MAC/bounds rejection,
  OpenProtocolInformation ABI, target relationships, failed/oversized queries
  and returned-buffer release. The common EFI report/handoff suite also passes.
- Stage: `tools/run-firmware-probe.py --http-cleanup-probe`.
- Verified one-time boot/collection: `tools/verify-http-cleanup.py reboot` / `verify`.
- Offline replay: research venv Python runs `tools/test-dell-http-lifecycle.py`.
  Assertions cover all nine cases; the imported entry replay checks four more.

Evidence under ignored `artifacts/`:

- `research/independent-bootstrap/lifecycle-tests.json`: traces, allocations,
  protocols, opens, HII package counts and injected failure outcomes.
- `firmware/http-cleanup-observation.txt`, SHA256
  `b073652d4d160e588d7210b0bc2e24929cbcbe99d72078d23edac0db6126f0ce`.
- `firmware/http-cleanup-probe-deployment.json` and boot-entry reports.
- Before/after backups: `20260927T151159Z` and `20260927T151313Z`.

EFI image SHA256:
`7ccce506d4581823c3faed4aacc6e4eb7188d21ef018915cf4d03c29e5c2d89e`.
Temporary Boot0004 was removed after collection. BootOrder `0005,0000` and
DriverOrder `0000,0001` are restored, BootNext absent and BootCurrent `0000`.
The owner NVRAM payload and all management/recovery loader hashes were verified
unchanged. Startup and recovery remain SSD-dependent.

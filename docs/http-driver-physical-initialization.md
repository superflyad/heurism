# HTTP boot driver initialized on the physical Dell

On 2026-09-27, Companion loaded and started the existing Dell HttpBootDxe through
an SSD-launched UEFI application. LoadImage and StartImage both returned
EFI_SUCCESS. The loaded image registered two driver-binding interfaces, for
IPv4 and IPv6. The probe removed both bindings and their component-name
protocols successfully, then management returned with trusted root SSH on boot
`149f6b3c-138f-47d0-bf0c-0816f077a8cb`.

This establishes targeted physical driver initialization. It does not establish
controller attachment, a working HTTP LoadFile provider, an HTTP transfer, or
SSD-independent startup. The driver registrations were temporary, and normal
DriverOrder was not changed to add this driver.

## Gates before execution

The probe first repeated the read-only prerequisite inventory, then applied
these execution gates within the same boot:

- No existing binding with the HttpBootDxe file identity was present.
- The policy interface was inside the expected DellBoardPolicyDxe image at
  RVA `0xc38`, with its method at `0x894`. The complete lookup instruction
  range `0x894..0x9c7` matched the saved provider byte for byte.
- A bounded memory-map and linked-list inspection found zero pending and three
  active policy callbacks. Neither list contained the HTTP request GUID
  `a686d83e-e7e7-4c01-8335-6fccb8e7c024`. Nodes required the expected signature,
  readable memory ranges, consistent back links and termination within 128
  nodes. Only counts and match status were reported.
- Exactly one accessible FV supplied the hash-pinned driver PE and dependency
  section. The actual FV-read PE bytes, not the comparison array, were given
  to LoadImage. The firmware file device path was retained.
- An ESP report was available before loading the driver.

The callback check is a snapshot immediately before initialization. It does not
instrument the live vendor function's return values or provide isolation from
all firmware activity. The actual saved lookup implementation returns NOT_FOUND
when neither list matches, which the saved driver constructor accepts. The
physical StartImage success confirms that initialization completed in this
tested state.

## Registration, cleanup and return

| Operation | Physical result |
| --- | --- |
| LoadImage from pinned FV-read buffer | EFI_SUCCESS |
| StartImage | EFI_SUCCESS |
| Bindings owned by the newly loaded image | 2, both version 10 |
| Component-name protocol removal | All four calls EFI_SUCCESS |
| Driver-binding protocol removal | Both calls EFI_SUCCESS |
| Remaining bindings owned by that image | 0 |
| Cleanup errors | 0 |
| Root SSH and management watcher after boot | Healthy |

The application does not explicitly call ConnectController, binding Start,
LoadFile, HTTP Request, DHCP Start, global firmware dispatch, or vendor unload.
UEFI StartImage itself can connect newly created/modified handles as part of
its compatibility behavior; this experiment does not trace firmware-internal
service calls. Controller attachment and transfer therefore remain separate
gates, rather than inferred results.

The saved vendor unload routine enumerates controllers, invokes
DisconnectController and calls an unverified packed cleanup protocol. The
probe avoids it. Instead it removes only binding and component-name interfaces
owned by the newly loaded image, and retains the image's boot-service memory
until ExitBootServices. It does not free memory while references might remain.
If removal fails, the probe saves the report and requests a cold firmware reset
through unchanged SSD defaults. That branch passed a host fixture but was not
deliberately induced on hardware. The existing 60-second UEFI watchdog also
remains during the experiment; it is not an independently proven reset channel.

Before reboot, the normal/recovery loaders, NV startup loader, root health,
management services and default orders were verified and backed up. The
temporary owner entry `0004` was selected once. Following return, its completed
report was collected, the entry removed and BootOrder restored to `0005,0000`.
DriverOrder remains `0000,0001` and BootNext is absent. BootCurrent was `0000`,
the verified SSD fallback loader. Report completion and healthy return are
observed; every step of the inner management-loader handoff is not traced.

The existing NVRAM owner payload is unchanged. This test performs no flash
programming and provides no control of a Dell firmware halt screen.

## Tests and evidence

Build with `tools/build-firmware-probe.ps1 -HttpInitProbe`. Eleven host cases
cover successful initialization/removal, matching policy callback rejection,
changed live lookup code, memory-map/list bounds, corrupted driver bytes,
load/start/remove failures, absent report and malformed firmware paths.
Execution, controller attachment and vendor unload callbacks not used by the
fixture remain unset. The common host suite also verifies EFI ABI, report
handling and the SSD GRUB handoff. The four saved-entry machine-code replay
cases were rerun and passed.

Deployment stages with `tools/run-firmware-probe.py --http-init-probe`;
`tools/verify-http-init.py reboot` checks installed recovery files before the
one-time selection, and its `verify` action collects and cleans up after a new
healthy boot. The completed report is retained; staging refuses to overwrite it.

Evidence under ignored `artifacts/firmware/`:

- `http-init-observation.txt`, SHA256
  `ba5f165c143158551c09f540a598ec36f766b8a8b1fde505c057eddd749af98c`.
- `http-init-probe-deployment.json` and before/staged/after boot-entry reports.
- Before-test boot-variable backup `20260927T144439Z/snapshot.json` and
  post-test backup `20260927T144545Z/snapshot.json`.

Physical probe image SHA256:
`5d3f54a83d9d716e0f516602336f300afae6547421a5e9395eb19a9dbb387633`.

The [Ethernet compatibility gate](http-ethernet-support.md) adds targeted
Supported checks and investigates cleanup before controller attachment.
The next HTTP boot gate is targeted controller attachment and cleanup,
followed by a transfer whose waits and cancellation behavior are verified.
The earlier [UDP network transfer](preboot-network-testing.md) already proves
bounded delivery and execution of the owner extension through an SSD-assisted
bootstrap; it does not prove this HTTP path. Native Dell PXE selection and SSD
removal remain excluded while preserving management access.

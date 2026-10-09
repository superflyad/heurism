# HTTP boot driver: dispatch and controller prerequisites

The investigation found a concrete driver-loading gate. The saved Dell
HttpBootDxe dependency section starts with **EFI_DEP_SOR**, schedule on request.
This is consistent with the driver being present in the ROM but absent from
the bindings inspected during normal SSD startup. It does not prove which gate
prevented its publication on this particular boot.

The driver's containing firmware volume, `b92cf322-8afa-4aa4-b946-005df1d69779`,
was exposed as a named FVB handle in the completed physical inspection. We
therefore have a specific existing volume/file pair to investigate, rather than
needing to invent a flash write path. The file is
`ecebcb00-d9c8-11e4-af3d-8cdcd426c973`.

## Evidence and offline tests

The 219-byte DXE dependency body was extracted from the previously pinned
saved ROM with the verified UEFIExtract revision, then copied and independently
hashed locally. It contains SOR, TRUE, twelve PUSH protocol GUIDs, twelve ANDs
and END. With all twelve protocol-presence fixtures true, the expression is
true; removal of any one makes it false. These are fixtures, not a fresh live
measurement of those architectural protocols.

The standard [dependency opcode definitions](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Pi/PiDependency.h)
and [DXE dispatcher implementation](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Core/Dxe/Dispatcher/Dispatcher.c)
explain the distinction: Schedule changes a matching unrequested driver to a
dependent state; Dispatch loads and starts eligible drivers. Dispatch is not
restricted to just the requested file. We have not called either service on
the physical Dell during this investigation.

The actual saved Dell driver has two driver-binding structures:

| Family | Structure RVA | Supported RVA | Start RVA | Stop RVA |
| --- | --- | --- | --- | --- |
| IPv4 | `0xc188` | `0xf54` | `0xffc` | `0x13e0` |
| IPv6 | `0xc158` | `0x1510` | `0x15b8` | `0x1a30` |

`tools/test-dell-http-binding.py` runs only each bounded Supported function in
Unicorn 2.1.4. Its single external service, OpenProtocol, is explicitly mocked
with TEST_PROTOCOL attributes. The machine code checks, in order:

1. DHCP4 or DHCP6 service binding on the controller.
2. HTTP service binding on that same controller.
3. Device path on that same controller.

Fourteen cases pass: all present, then each prerequisite returning unsupported
or access denied. Failures propagate immediately without testing later
prerequisites. The tests validate the argument layout, controller and agent
handles, null interface output and test-only attributes. They do not run the
driver entry point or Start, touch the adapter, perform DHCP, or download data.

The existing physical report establishes an HTTP binding on a MAC-bearing
controller. It does not establish that all three prerequisites are co-located
on that handle. That is a specific remaining inventory check. A missing HII
form also cannot alone distinguish an unloaded driver from one which has not
started on a controller; the reference
[HttpBootDxe implementation](https://github.com/tianocore/edk2/blob/master/NetworkPkg/HttpBootDxe/HttpBootDxe.c)
initializes its form during Start. This source describes the reference design,
not a proof that Dell's complete initialization has identical side effects.

The seven previously surveyed saved bootstrap modules were also searched for
the HTTP boot file GUID. Only HttpBootDxe itself contains its exact bytes.
No requester was identified in those seven modules. Computed/indirect requests
and other unexamined modules remain possible.

The follow-up [entry and policy-provider investigation](http-driver-initialization.md)
expands that search to the extracted ROM, replays the complete driver entry
with explicit service fixtures, and tests the real provider's default-table
lookup. It also narrows the interpretation of the earlier HTTP rejection.

## Safe experiment sequence

For SSD-assisted loading, the next probe should first verify the firmware
volume/file's readable PE and dependency sections and check DHCP/HTTP/device
path co-location. The saved driver's entire entry/constructor path must be
reviewed before allowing it to execute. Targeted LoadImage/StartImage of the
existing FV file is a candidate alternative to global DXE Dispatch, subject to
firmware authentication and initialization requirements. Neither route has
been physically tested here, and a synchronous vendor call is not made bounded
merely by putting a timeout around our own loop.

After publication, compatibility, controller attachment, trusted HTTPS
transfer and downloaded-image execution remain separate tests. Every physical
experiment must enter through the proven SSD path, retain its recovery files
and boot defaults, and explicitly return to management on handled failure.
No native network boot selection or SSD removal is needed for these stages.

For SSD-independent startup, a second gate remains: Dell's own startup sequence
must request/load the driver and select a durable owner network boot profile.
A successful manual load from Companion would not prove either. NVRAM can
retain configuration, but these findings do not establish an automatic
configuration route on this 7506. Root SSH still begins only after a management
OS boots; it does not control a firmware halt screen.

## Reproduction and management state

Run `artifacts/research-venv/Scripts/python.exe tools/test-dell-http-binding.py`
and `python tools/analyze-http-dispatch.py`. The second script reads the saved
dependency body from the Dell if its local copy is absent; it never changes
firmware configuration. Evidence is under ignored
`artifacts/research/independent-bootstrap/`: `binding-tests.json`,
`dispatch-analysis.json` and `http-boot-depex.bin`.

Module SHA256:
`d992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788`.
Dependency SHA256:
`b73f346c1fb9a891c672d684f5d6eceb36ba1371c6f6f77bd2e6bbfa569a7cdf`.

The pinned root connection remains healthy on boot
`b8c1de82-8a89-42de-945f-17d82af25c04`, with SSH and companion-watch started,
BootOrder `0005,0000`, DriverOrder `0000,0001` and no BootNext. This investigation
performed no reboot, driver execution on hardware, boot-variable change,
certificate enrollment or flash programming. Remote writes only extracted a
section of the already saved ROM into the research directory.

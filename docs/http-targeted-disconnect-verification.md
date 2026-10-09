# Targeted HTTP driver disconnect verification

On 2026-09-27, eight offline cases passed using the saved Dell DXE
DisconnectController dispatcher and the saved HttpBootDxe Supported, Start
and Stop code. The nominal targeted route selects the newly created DHCP
child and removes the driver's owned interfaces. Existing firmware interfaces
and the existing DHCP child remain unchanged in every fixture.

This verifies a candidate cleanup route in a modeled environment. It does not
establish safe physical attachment: four injected cleanup failures still return
success while leaving state behind. No driver was physically attached and no
reboot or firmware mutation was performed during this verification.

## Code and execution boundary

The pinned DXE core is `artifacts/firmware/update-modules/dxe-core.bin`, SHA256
`8641816dae6964f620aeac118b0303376f1abe0d3e6be6f066e17cbf52d6c246`.
Its saved boot-services table at RVA `0x291d0` identifies DisconnectController
at RVA `0x4a2c` through table offset `0x110`. The dispatcher body executes
unmodified after PE mapping and DIR64 relocation.

The pinned HttpBootDxe SHA256 is
`d992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788`.
Its Supported, full Start and Stop routines execute unmodified. The test uses
an explicit driver handle and a NULL child parameter to DisconnectController.
It does not exercise a broad disconnect of all drivers.

The DXE handle database is synthetic, built from fixture protocol/open records.
Core handle validation, protocol lookup, locking and pool allocation/free are
explicit helper fixtures. HTTP service, HII and boot-services dependencies are
the lifecycle fixtures described in [the cleanup investigation](http-cleanup-investigation.md).
Unknown core helper calls fail the replay. This is not execution of the full
DXE core or a measurement of live core code ownership. The
[upstream EDK II dispatcher](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Core/Dxe/Hand/DriverSupport.c)
provides structural context; the tested dispatcher is the pinned Dell binary.

## Results

All cases first run Supported and Start successfully. Failure injection applies
to cleanup, and wrong-target cases deliberately exercise the dispatcher despite
being rejected by the modeled preflight.

| Target or injected failure | Dispatcher result | Observed cleanup | Offline handoff gate |
| --- | --- | --- | --- |
| New owned DHCP child | Success | Stop called once on that child; owned protocols, opens and HII package removed | Pass |
| Original NIC | Not found | Stop not called; six new protocol interfaces remain | Reject |
| Existing firmware DHCP child | Not found | Stop not called; six new interfaces remain; existing child preserved | Reject |
| Invalid handle | Invalid parameter | Stop not called; six new interfaces remain | Reject |
| HTTP child interface removal fails | Success | Two interfaces remain; one points into freed backing memory | Reject |
| HII interface removal fails | Success | Two interfaces remain; both point into freed backing memory | Reject |
| DHCP child destruction fails | Success | One protocol interface remains | Reject |
| HII package removal fails | Success | HII package remains despite interface removal | Reject |

Nominal cleanup retains a 72-byte child device-path allocation, matching the
earlier lifecycle replay. No protocol reference to it remains in that fixture.
The test does not treat the retained allocation as a dangling interface or
claim that all allocations were freed.

The modeled preflight requires one exact BY_DRIVER DHCP open record owned by
the new driver binding, with the MAC-selected NIC as ControllerHandle. Its
child handle must be absent from the baseline. Postconditions require removal
of all new protocol interfaces, owned opens and HII packages, destruction of
the created DHCP child, and preservation of baseline firmware state. These
decisions are offline assertions, not a deployed EFI recovery policy.

## What remains before physical attachment

Targeted DisconnectController does not repair the vendor's ignored cleanup
errors. A safe next implementation must either retain backing memory when
removal fails, or use a validated bounded recovery path before management
handoff. An autonomous default-SSD reset after such a failure is still unproven.
Do not physically attach this driver based only on nominal replay success.
Never disconnect the existing firmware DHCP child or use the vendor unload
routine as a shortcut.

The Dell's root management connection was checked after the replay and remained
healthy on boot `73ece2ba-139c-4c0c-8369-0c795cd39c40`, with sshd and the
Companion watcher running. BootCurrent was `0000`; BootOrder `0005,0000` and
DriverOrder `0000,0001` remained intact, with BootNext and temporary Boot0004
absent. Management/recovery still depend on the SSD. There is no demonstrated
independent reset channel or Ethernet control of a firmware halt screen.

## Reproduction and evidence

Run `artifacts/research-venv/Scripts/python.exe tools/test-dell-targeted-disconnect.py`.
The targeted suite asserts eight cases; importing its lifecycle dependency also
runs the four existing entry replay checks.

The report is `artifacts/research/independent-bootstrap/targeted-disconnect-tests.json`,
SHA256 `738ac9cd0bce445d94de094a0524c30bc61a8c99dc7c01c5803f907f9c96ebf2`.
It records helper calls, Stop targets, protocol/open state, remaining allocations,
failure outcomes and the offline handoff decisions. Artifacts are ignored by Git.

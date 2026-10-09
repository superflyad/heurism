# Ethernet compatibility gate

On 2026-09-27, the corrected probe completed on physical boot
`baacdd87-1731-4568-8a13-b7732b226f96`. Both IPv4 and IPv6 Supported calls
returned EFI_SUCCESS. Four component-name interfaces and both owned driver
bindings were removed successfully; zero owned bindings remained and cleanup
errors were zero. Root SSH and the management watcher returned healthy.

BootCurrent was `0000`, the verified SSD fallback. Temporary entry `0004` was
removed, BootOrder restored to `0005,0000`, DriverOrder remained `0000,0001`,
and BootNext was absent. No controller attachment or HTTP transfer was tested.

Companion's Ethernet probe initializes the pinned Dell HttpBootDxe image and
calls its IPv4/IPv6 Driver Binding Supported methods on the unique HTTP service
controller with MAC `7c:c2:c6:1d:b2:f5`. It requires DHCP4/DHCP6 services on the
same handle, the expected loaded-image identity, binding offsets, version
**decimal 10**, and exact method addresses. It does not call controller Start,
Stop, ConnectController, LoadFile, DHCP Start or HTTP Request.

The shared initializer checks firmware-section pins and live policy state,
then removes this image's binding and component-name protocols and resumes
SSD management. It avoids the vendor unload callback and retains the image
until ExitBootServices. The normal boot entries and driver order are preserved.
This remains dependent on the SSD and does not provide access to BIOS halt
screens or an independently proven reset channel.

## Why attachment is a separate gate

The later [cleanup investigation](http-cleanup-investigation.md) executes full
Start/HII setup and Stop offline and inspects physical protocol open records.
It identifies ignored removal failures and a candidate targeted disconnect
route. Physical attachment remains untested.

The saved IPv4 Stop implementation at RVA `0x13e0` first looks for LoadFile on
the supplied handle. Otherwise it searches that handle's MNP/DHCP4 protocol
open records for a BY_DRIVER entry and uses its ControllerHandle to find the
NIC's private interface. If no record resolves the NIC, Stop returns success
without destroying anything. Its return status alone is insufficient evidence
of cleanup.

`tools/test-dell-http-stop.py` executes the actual saved Stop instructions in
Unicorn against two explicit attached-state fixtures. In the original-NIC
fixture, which deliberately lacks those protocol open records, Stop returns
success with the private interface and HTTP child still present. In the
DHCP-child fixture, its BY_DRIVER record resolves the NIC and Stop removes the
HTTP/private interfaces, destroys DHCP child and frees private state.

These fixtures do **not** execute full Start, reproduce live NIC open records,
or exercise initialized HII cleanup. They do not establish that passing a
DHCP child directly satisfies the UEFI calling restrictions. Before a physical
attachment test, replay complete Start and HII cleanup, inspect the actual open
relationships, and require evidence that children, private interface and owned
open records are removed. Preserve the SSD paths throughout.

The upstream [EDK II driver implementation](https://github.com/tianocore/edk2/blob/master/NetworkPkg/HttpBootDxe/HttpBootDxe.c)
documents Supported-before-Start and valid-controller requirements. The actual
Dell module, rather than upstream similarity, supplies the tested instructions.

## Reproduction

- Build: `tools/build-firmware-probe.ps1 -HttpEthernetProbe`.
- Host tests: 19 cases covering shared initialization guards and cleanup,
  unique MAC selection, duplicate/wrong MAC rejection, decimal version and
  method identity, Supported calling convention/status and SSD handoff.
- Saved-code Supported replay: `tools/test-dell-http-binding.py`, 14 cases.
- Saved-code Stop replay: `tools/test-dell-http-stop.py`, two fixtures; the
  imported entry replay also runs its four cases.
- Stage: `tools/run-firmware-probe.py --http-ethernet-probe`.
- One-time boot and collection: `tools/verify-http-ethernet.py reboot` / `verify`.

Reports are create-only. The first physical run safely rejected an incorrect
hexadecimal version comparison before calling Supported; its report and
deployment metadata were archived as `http-ethernet-01-*`. The corrected second
profile has a distinct EFI/report filename and tag. It adds regression coverage
for version 10 versus `0x10` and does not overwrite the first report.

Physical evidence under ignored `artifacts/firmware/`:

- `http-ethernet-observation.txt`, SHA256
  `f6ca0306de5e1656c8fdd59aabe48a85507fc5d6440bd8eeca9deaa691d34fd3`.
- `http-ethernet-probe-deployment.json` and before/staged/after boot reports.
- Backups before corrected probe: `20260927T150218Z/snapshot.json`; after
  cleanup: `20260927T150339Z/snapshot.json`.
- Corrected EFI image SHA256:
  `c0023ee65a631222024b551ebe636c8c98adba814c297b35acf88b546b15950d`.

The common initializer rebuilt to exactly its previously tested image digest,
and its eleven host cases still passed after adding the optional hook.

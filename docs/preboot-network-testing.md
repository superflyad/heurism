# Safe preboot network tests on target 0

Companion can now transfer and execute its exact owner extension over Ethernet
before Linux starts. A second physical boot with the external test server stopped
returned to healthy root SSH after bounded receive failures. The SSD stayed
enabled throughout: this establishes a network transport and safe fallback,
not a fully SSD-independent startup or an out-of-band management channel.

## Native interface inventory

Boot `80a02ba1-8060-4cec-be4f-f852ef75a68d` ran `boot/network_probe.c`, a read-only
inventory. It discovered three SNP handles for the same connected Realtek MAC
`7c:c2:c6:1d:b2:f5` (base, IPv4 and IPv6 paths), two PXE/LoadFile handles, one
HTTP service binding, one IPv4 configuration provider, and one UDP4 service
binding. These are protocol handles, not three physical adapters. The NIC was
initialized and reported media present. No network transaction was started.
Root SSH and the watcher returned healthy, and its temporary entry was removed.

## HTTP result and controller preparation

Boot `926bedd7-0c16-42b9-a332-261e8e321221` created and configured HTTP children
successfully, but Request returned `EFI_ACCESS_DENIED` (`0x800000000000000f`)
for both URLs before any transfer. Reset and child destruction succeeded.
No network image was executed. Root management returned healthy.

EDK II has a build-time `PcdAllowHttpConnections` policy whose default denies
plaintext HTTP. This is a possible explanation for the Dell result, not a
verified attribution to that exact implementation or policy value. No firmware
policy or TLS trust store was changed. The separately exposed UDP interface
was tested next.

Later [saved-transport instruction tests](http-driver-initialization.md) found
HTTP progressing through URI classification and a distinct duplicate-token
ACCESS_DENIED gate. Those bounded offline slices do not attribute the physical
rejection, but the plaintext-policy explanation remains unconfirmed and should
not be treated as its established cause.

The controller runs the fixed read-only server in
`tools/preboot-http-server.cjs`. It binds this Windows computer at
`10.8.22.122:18080` for health/fixed HTTP routes and UDP `:18081` for the owner
transfer. It accepts only the target's address for UDP, has no command execution
or upload API, and expires after thirty minutes. Only the exact previously
verified 2 KiB driver and an intentionally corrupted copy are served.

The initial Python server was blocked by existing Windows firewall rules. This
session could not add a rule. The installed Node runtime at
`C:/Program Files/Adobe/Adobe Dreamweaver 2021/node/node.exe` had an existing
permitted rule for the active network; its server passed both live health
checks from the Dell. No firewall rule was added, removed or relaxed. The first
chosen TCP port was in a Windows exclusion range, so an available port was used.

## UDP implementation and safeguards

`boot/udp_transfer_probe.c` creates its own UDP4 child through standard firmware
services. It uses the target's already assigned address `10.8.22.238/24` and
local port 18082; deployment refuses a changed lease or a failed live controller
health check. It does not provide DHCP service or persist a network policy.

The fixed experiment protocol uses a 16-byte request: `CMPNET01`, a little-endian
32-bit chunk index (0 or 1), and a mode (0 exact, 1 corrupted). The response echoes
that header and carries 1024 payload bytes. Mode 99 is a readiness echo only.
The receiver checks address/ports, length, fragment bounds and the echoed header.
Each asynchronous wait is bounded to 300 polls with a 10 ms stall, after which
pending work is cancelled. Children are reset and destroyed and events closed.
The framework retains its 60-second firmware watchdog during the probe; a
firmware hang/reset at this stage was not deliberately tested.

The first transfer downloads the altered copy and must reject it. The second
downloads the exact copy; every byte must match the pinned reference before
LoadImage or StartImage is called. LoadImage receives the downloaded buffer,
not the embedded comparison image. HandleProtocol on the newly loaded child
and its GetInfo verify that child's service, so the existing SSD driver cannot
produce a false success. This narrow protocol is not a general remote shell
or an update mechanism for arbitrary code.

Host tests exercise successful delivery, altered-byte rejection, wrong response
identifiers, wrong source port, excessive fragment counts, receive timeout with
cancellation, configuration failure and cleanup. The x64 application has
relocations and no OS imports. Its SHA-256 is
`4a9694f8b5cde97855a76fc72abe807df0c7a7658dbcdf844b57abaab3a6ee03`.

## Physical online and offline tests

| Check | Online boot | Controller-offline boot |
| --- | --- | --- |
| Boot ID | `089c5a3a-f2b3-46da-9a56-95bb9097b19a` | `a7f66b32-1a47-4a47-b5ac-27407ce9efbe` |
| Altered payload | Received both chunks and rejected | Receive timed out |
| Exact payload | Received both chunks and matched | Receive timed out |
| LoadImage / StartImage | Both EFI_SUCCESS | Not called |
| New child protocol / GetInfo | EFI_SUCCESS; expected magic, revision and capability | Not called |
| Network child reset/destruction | Both children EFI_SUCCESS | Both children EFI_SUCCESS |
| Root SSH and watcher | Healthy | Healthy |

The Windows server independently logged all four native UDP exchanges from
the target's port 18082. Those were distinct from the OS readiness check.
For the failure test, its recorded process identity was checked and the server
was deliberately stopped before reboot. Both native receive operations returned
`EFI_TIMEOUT` (`0x8000000000000012`), and no downloaded image executed. No local
assistance was needed.

Final BootOrder is `0005,0000` with no pending BootNext. DriverOrder remains
`0000,0001`, retaining Dell setup and the original SSD extension. The temporary
inventory, HTTP and UDP boot entries are removed. The earlier board-stored owner
payload still has attributes 7 and the identical 2048-byte pinned hash. The test
server is stopped. Current root access is healthy.

Evidence is retained under ignored `artifacts/firmware/` in
`network-probe-verification.json`, `http-probe-verification.json`,
`udp-probe-verification.json`, their raw observation reports, and
`http-server-requests.jsonl`.

## What remains before SSD-independent control

The successful bootstrap itself still starts from an SSD EFI file. Its report
and normal/rescue boot files also use the SSD. The network-delivered code is a
small boot-service driver, not an independent management OS. PXE/LoadFile was
inventoried, but booting a network image directly from the firmware boot manager
has not been tested. The SSD has not been removed or disabled.

The next gate is to verify a bounded firmware-native network boot in isolation,
then a RAM management image that can return pinned root SSH without any SSD
files. Only after those pass should a physical test remove SSD dependency.
Current SSH cannot reset a machine halted inside firmware; preserving SSD
fallback does not create an out-of-band reset or BIOS-console channel. The
controller-offline test covers an unavailable transfer server, not a failed NIC,
failed switch, complete firmware hang or power loss.

Build using `tools/build-network-probe.ps1`, `tools/build-http-probe.ps1` and
`tools/build-udp-probe.ps1` with the existing explicit LLVM paths. Deployment is
`tools/run-firmware-probe.py` with exactly one probe flag. The UDP sequence is
`tools/verify-udp-probe.py verify`, `offline`, `verify`, then `finalize`.
These deployment records are single experiment records; inspect existing state
and preserve evidence before starting a new run.

Sources: [UEFI network protocols](https://uefi.org/specs/UEFI/2.10/24_Network_Protocols_SNP_PXE_BIS.html),
[official UDP4 ABI](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/Udp4.h),
[official HTTP ABI](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/Http.h),
and [EDK II HTTP connection policy](https://github.com/tianocore/edk2/blob/master/NetworkPkg/NetworkPkg.dec).

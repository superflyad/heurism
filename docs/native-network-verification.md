# Native network and management verification

Companion's native kernel drives QEMU's Intel 82540EM, exchanges ARP/IPv4/ICMP/UDP
packets and serves authenticated discovery, status and reboot commands after
ExitBootServices. The VM reboots through its kernel command path, returns to
the native loader, issues a fresh boot nonce and accepts a new session. No Linux
or EFI networking participates. This is virtual development evidence: the
physical Dell uses Realtek USB Ethernet and does not have this NIC.

## Reproduction and isolation

```powershell
python tools/build-kernel.py --vm-diagnostics --vm-network --out build/native-network
python tests/network_host.py
python tests/native_network_smoke.py
```

Full `tests/native_acceptance.py` includes network proof and the existing seven
native boot/input/failure cases, deterministic rebuilds and production exclusion
checks. QEMU's TCP socket backend binds to `127.0.0.1` and carries raw Ethernet
with length framing. No LAN bridge, TAP adapter or physical route is created.
The VM is always terminated on completion/failure. See
[QEMU socket documentation](https://www.qemu.org/docs/master/system/qemu-manpage.html)
and [framing implementation](https://github.com/qemu/qemu/blob/master/net/socket.c).

## NIC ownership

`drivers/net/e1000.c` accepts only the supported 8086:100e device/memory BAR.
It maps registers uncached, masks interrupts, stops/resets within a fixed poll
bound and allocates 32-entry RX/TX rings with owned buffers. Bus mastering is
enabled only after descriptors are initialized. PCI command updates use
16-bit accesses to avoid clearing status bits. Initialization failure before
DMA enable frees allocated pages and leaves bus mastering disabled.

Receive validates descriptor completion, packet end, errors and length;
multi-descriptor packet chains are discarded. Transmit requires completion
before reuse, pads short frames and requests writeback. The native timer polls
the rings. DMA teardown, physical PHY variants and IOMMU ownership are not
validated. References: [Intel 8254x manual](https://www.intel.com/content/dam/doc/manual/pci-pci-x-family-gbe-controllers-software-dev-manual.pdf)
and [QEMU e1000](https://github.com/qemu/qemu/blob/master/hw/net/e1000.c).

## Protocol and credentials

The fixture uses MAC `52:54:00:12:34:56`, IPv4 `10.0.2.15` and UDP port 47333.
ARP and ICMP echo are diagnostics. There is no DHCP, routing, fragment
reassembly, TCP, SSH or TLS. Packet/header lengths, destination, fragment flags
and IP/UDP checksums are checked before management parsing. UDP requires a
nonzero checksum. See [UDP checksum definitions](https://www.rfc-editor.org/rfc/rfc768).

Requests are exactly 80 bytes, with network-order integers:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | `CMP1` magic |
| 4 | 1 | Version 1 |
| 5 | 1 | Operation: 1 discovery, 2 status, 3 VM reboot |
| 6 | 2 | Reserved zero |
| 8 | 8 | Sequence |
| 16 | 16 | Boot nonce |
| 32 | 16 | Nonzero client challenge |
| 48 | 32 | HMAC-SHA256 over bytes 0–47 |

Discovery uses sequence zero and a zero request boot nonce. Its signed response
contains the current boot nonce and echoes the challenge. Status/reboot require
that nonce and a positive sequence above the last accepted sequence. Invalid
requests do not advance it. Responses set the operation's high bit and append
five 64-bit values: ticks, free pages, received frames, submitted frames and
last sequence. HMAC covers all 88 header/body bytes; total response size is 120.

Boot nonces use two successful RDRAND operations with bounded retries. The
fixture uses `-cpu max`; unavailable entropy support disables management.
This is not an RNG certification. Reboot is scheduled after the signed reply
enters TX and waits for completion, cancelling on timeout. The reset uses q35
port 0xcf9 and is not a Dell reset-channel claim.

The key is **public fixture data**, bytes 0 through 31. It is not a deployment
credential. `--vm-network` requires `--vm-diagnostics`, including under Python
optimization. Production compiles the fixture/reset module out and enables no
endpoint. Do not deploy the fixture to a LAN or the Dell. Production credential
provisioning, confidentiality and broader authorization remain future work.
Commands provide bounded status/reboot, not a shell or arbitrary memory,
firmware or storage writes. Algorithm references:
[NIST SHA-256](https://csrc.nist.gov/pubs/fips/180-4/upd1/final) and
[HMAC](https://www.rfc-editor.org/rfc/rfc2104).

## Observations

Host C checks pass 160 SHA/HMAC comparisons against Python hashlib/hmac across
padding boundaries, long messages and key sizes, plus 2,686 invalid frame/auth
cases. These include bit flips, correctly signed malformed requests, replay and
cross-boot nonce mismatch; output canaries and valid ARP/ICMP/commands pass.

Actual VM NIC checks pass 80 packet round trips across more than two ring
rotations. Bad HMAC/IP/UDP checksums, replay, wrong nonce/port, unknown signed
operation, fragments, truncation and omitted UDP checksums get no response.
Authenticated kernel reboot produces two native startups and a fresh nonce.
Captured old status/reboot packets are rejected and fresh status succeeds after
reconnect. No QMP reset is used; QMP freezes the guest only after observations.

Reports: `build/native-network/network-host-verification.json` and
`vm-network/verification.json`, with serial traces alongside. Acceptance records
hash reports and binary manifests. No physical Realtek support, firmware-pause
control or recovery from a total kernel hang is established.

The first [xHCI/USB descriptor inspection](native-usb-verification.md) now passes
host and VM checks. [USB bulk/CDC ECM management](native-usb-network-verification.md)
also passes packet and reboot/reconnect checks. Next: the physical Intel controller,
identified Realtek adapter, production keys and Dell-verified recovery/reset. Preserve SSD Linux
management/recovery defaults.

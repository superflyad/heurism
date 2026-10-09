# Native USB Ethernet verification

The native kernel now carries authenticated management traffic through owned
xHCI bulk endpoints and a CDC ECM USB Ethernet device in an isolated QEMU VM.
It boots through the Companion EFI loader, exits firmware boot services, and
handles packets and an authenticated reboot using kernel code. Linux and GRUB
are absent from this VM path. The physical Dell still boots its verified Linux
management environment; these native drivers have not run there.

## Implementation

`drivers/usb/xhci.c` retains one addressed root-port device, submits exact-length
IN/OUT/no-data control requests, and configures separate bulk IN/OUT contexts,
transfer rings and buffers. It validates endpoint direction and packet size.
Ring cycle ownership, completion pointer/slot/endpoint, residual lengths and
completion codes gate buffer use. Each bulk endpoint has one outstanding TD.
An IN NAK leaves the TD pending without a blocking traffic wait. A completed
zero-length transfer remains visible to the caller as a frame boundary.

For an OUT frame whose length is a multiple of endpoint packet size, a separate
zero-length TD terminates the USB frame. Chaining a zero-length TRB into the
data TD did not produce that transaction in the VM; the packet tests exposed
the bug and now exercise exact packet-size boundaries across ring wraparound.
Controller failure stops networking. Halt precedes freeing DMA; an uncertain
halt retains owned memory rather than allowing reuse during possible DMA.

`drivers/net/usb_ecm.c` validates a CDC ECM configuration, the control/data Union,
Ethernet descriptor, alternate setting, bulk endpoints and UTF-16 hexadecimal
MAC string. It selects the configuration and data alternate, then enables the
directed/broadcast packet filter. The parser supports full/high/SuperSpeed ECM;
it does not implement RNDIS, CDC notifications, USB hubs or Realtek vendor
commands. Oversized receive frames are discarded through the next short
transfer or ZLP so a signed-looking tail cannot become a new management frame.

`kernel/vm_management.c` shares the existing authenticated UDP status/reset
fixture between e1000 and USB ECM. The key bytes 0..31 are public test data.
Both profiles compile out of production. The build rejects USB networking
without diagnostics and USB support, and rejects simultaneous e1000/USB network
profiles. Its q35 port-0xcf9 reset is a VM behavior, not a Dell reset guarantee.

The implementation follows Intel's
[xHCI specification](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf).
The ECM fixture's descriptors and USB packet termination are checked against
[QEMU's USB network implementation](https://github.com/qemu/qemu/blob/master/hw/usb/dev-network.c).
QEMU's USB network input buffer caps frames at 2048 bytes. The VM can test an
oversized full transfer followed by ZLP; draining an oversized frame across
several full TDs is separately tested in the actual host C boundary function.

## Reproduce and evidence

```powershell
python tools/build-kernel.py --vm-diagnostics --vm-usb --vm-usb-network --out build/native-usb-network
python tests/native_host.py --build build/native-usb-network
python tests/usb_host.py --build build/native-usb-network
python tests/native_network_smoke.py --transport usb_ecm --build build/native-usb-network
python tests/native_acceptance.py
```

The loopback TCP backend carries raw Ethernet only between this test controller
and QEMU. It creates no LAN listener or physical-device connection.

| Check | Evidence |
| --- | --- |
| Native packets | ARP, authenticated UDP status and 140 ICMP round trips; exact USB packet-size boundaries and repeated IN/OUT ring wraparound |
| Rejection | Bad HMAC/checksums, replay, old nonce, unknown command, wrong port, fragmentation, truncation, missing UDP checksum and two oversized USB frames receive no reply |
| Reboot/reconnect | Authenticated kernel command performs reset; two native loader/kernel starts; fresh boot nonce; previous-boot commands rejected; fresh status accepted |
| Host driver failures | 68 controller cases: command/control/bulk completion failures, allocation failures, NAK pending, zero/full receive, copy-capacity guard, 32/64-byte contexts, SuperSpeed bursts, captured ECM setup and failed-halt DMA retention; 2,295 malformed descriptor cases |
| Host framing | 15 actual C boundary transitions, including multiple full oversized TDs followed by a signed-size tail, short termination and ZLP recovery |

Reports reside in `build/native-usb-network/usb-host-verification.json` and
`vm-usb-network/verification.json`, with serial trace and binary manifest. The
full acceptance report at `build/native/acceptance.json` binds its thirteen VM
case reports by SHA256, checks deterministic builds and production exclusions.
Read the reports for the exact tested binary hashes; a later edit requires new
verification before those results apply.

## Physical boundary and next work

The Dell's TP-Link adapter is 2357:0601, uses Linux's r8152 driver and attaches
at SuperSpeed to Intel 8086:a0ed xHCI. Its vendor configuration differs from
QEMU's 0525:a4a2 adapter, but a read-only capture found a second CDC ECM
configuration. [Captured ECM evidence](dell-usb-ecm-investigation.md) now passes
host parsing/setup models; physical traffic remains untested.
The wrapper still permits only QEMU 1b36:000d, and this build must not be selected
on the Dell. Remaining work includes physical Intel ownership/IOMMU validation,
physical ECM traffic/initialization or Realtek vendor support, protected production credentials,
and a Dell-tested bounded recovery/reset path before switching native kernels.

The read-only reference check in
`build/native-usb-network/dell-management-after.txt` records healthy root boot
7c465eeb-59ca-46c5-8f6a-31954e668b8c, SSH/watch started, BootOrder 0005,0000,
DriverOrder 0000,0001, absent BootNext, six protected hashes passing and
`companion_pending=0`. No physical reboot or native deployment occurred in
this implementation batch. Existing management startup remains SSD-dependent;
Ethernet cannot control a firmware warning screen or a total kernel hang.

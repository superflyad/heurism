# Dell adapter CDC ECM route

A read-only inspection of the Dell's actual Ethernet adapter found two USB
configurations. The working Linux management link uses vendor configuration 1.
Configuration 2 advertises standard CDC ECM, offering a candidate route for the
native kernel's existing ECM driver. No configuration was changed on the Dell,
and physical ECM packet traffic remains untested.

## Captured hardware evidence

The reference connection was healthy root boot
`7c465eeb-59ca-46c5-8f6a-31954e668b8c`. Linux reported TP-Link `2357:0601`,
`bcdDevice=3000`, SuperSpeed 5000 Mb/s, r8152 v1.12.13 and firmware
`rtl8153a-3 v2 02/07/20`. It exposes two configurations and selects value 1.
The binary USB descriptors were read from the kernel's sysfs descriptor file.
No direct register requests, driver detach, USB reset or reboot were used.

The captured fixture is `tests/fixtures/dell-realtek-usb.json`. Its 173 bytes
include the device descriptor and complete 57-byte and 98-byte configurations.
The fixture binds those raw bytes to SHA256. Its metadata records the source,
boot identity, active configuration and scope. The original read output is
`build/reference/dell-realtek-descriptors.txt`.

| Configuration | Observed descriptors |
| --- | --- |
| 1, currently active | One vendor interface; EP1 IN and EP2 OUT, 1024-byte bulk packets and MaxBurst 3; EP3 interrupt |
| 2, candidate ECM | CDC control interface 0, ECM subclass 6, Header 1.10, Union linking data interface 1, Ethernet descriptor with MAC string index 3 and 1514-byte segment size |
| ECM data interface | Alternate 0 has no endpoints; alternate 1 has EP1 IN and EP2 OUT, 1024-byte bulk packets and MaxBurst 3 |

The presence of these descriptors is hardware evidence. It does not prove PHY
initialization, working packet traffic, reconnect, firmware ownership handoff
or recovery under our native kernel.

## Source check and implementation

Linux's [r8152 configuration selector](https://github.com/torvalds/linux/blob/v6.18/drivers/net/usb/r8152.c)
chooses a vendor-class configuration when its hardware-version check recognizes
the device. The separate
[generic CDC Ethernet driver](https://github.com/torvalds/linux/blob/v6.18/drivers/net/usb/cdc_ether.c)
binds the CDC control/data interfaces and enables the directed/broadcast filter.
These sources and the captured descriptors support investigating ECM first;
they do not establish that this physical adapter will work without additional
initialization. The reference source download is
`build/reference/linux-v6.18-r8152.c`, SHA256
`9d27b82f19e4d6e20232fd7bba17c8a79ad6c00b48a170ba99affbb79537e0a6`.
The running Dell kernel is 6.18.53; the v6.18 reference is not claimed to be its
exact compiled source.

`drivers/net/usb_ecm.c` now parses full/high/SuperSpeed ECM configurations.
SuperSpeed endpoint companions must immediately follow their endpoint, have
valid packet/burst bounds, and describe no streams on bulk endpoints. Interrupt
companion interval bytes are checked against their payload budget. The native
transport has one bulk ring per endpoint, so streams are rejected. The fixed
Ethernet frame budget requires a 1514-byte segment descriptor. A CDC Header,
Union, Ethernet functional descriptor and matching data endpoints are required.
The companion layout was also checked against Microsoft's
[SuperSpeed descriptor documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/usbspec/ns-usbspec-_usb_superspeed_endpoint_companion_descriptor).

The actual C parser rejects vendor configuration 1 and recognizes configuration
2 from the saved hardware bytes. A host model then exercises actual
`usb_ecm_start` and xHCI code through the captured configuration queries, MAC
string query, configuration 2 selection, data alternate 1 selection and packet
filter setup. Every setup control step also has an injected failure case;
the caller halts the controller before releasing DMA. The MAC string reply in
this model is synthetic, using the known adapter MAC; it was not captured from
a physical GET_DESCRIPTOR request.

Bulk host models also check EP1 IN/EP2 OUT, 512-byte high-speed and 1024-byte
SuperSpeed packet sizes, burst 3, both 32/64-byte xHCI contexts, and wraparound
with terminating ZLP TDs. These are callback/DMA models, not physical transfers.
There are 68 controller cases, 2,295 malformed descriptor cases and 15 framing
transitions. The SuperSpeed descriptor subset includes 124 rejection cases.

Reproduce with `python tests/usb_host.py --build build/native-usb-network`.
The report records the captured fixture and actual C source hashes. The complete
`python tests/native_acceptance.py` suite also reruns the existing 13 VM cases,
including USB ECM packet/reboot tests on QEMU's full-speed adapter. QEMU does
not emulate this Dell Realtek configuration or physical SuperSpeed traffic.

## Next physical gate

Retain the working vendor configuration and SSD management/recovery defaults.
Read-only Linux resources identify the controller's BAR at
`0x601f260000..0x601f26ffff`: a 64-KiB aperture above the native kernel's current
64-GiB identity map. Both the mapping limit and the QEMU-only 16-KiB controller
profile must be addressed before physical ownership. No Linux IOMMU group was
present for this device; DMAR and interrupt-remapping logs were present. That
does not prove DMA translation is inactive at firmware handoff. Evidence is
`build/reference/dell-intel-xhci-resources.txt`.
These are Linux resource assignments; read the actual preboot PCI BARs rather
than assuming this address will be unchanged at native handoff.

The native wrapper still accepts only QEMU's controller. Before a physical
native run, establish the Intel controller's mapped aperture, ownership and
IOMMU handling, production management credentials and a bounded recovery/reset
path. Then test the captured ECM configuration with independent observations
of packets and reconnect. If ECM needs vendor initialization or fails under
the native driver, implement the relevant RTL8153 vendor path from verified
hardware revision evidence. Current SSH cannot recover a total native hang or
dismiss a firmware error screen.

The final read-only check at `build/reference/dell-ecm-management-after.txt`
confirms root SSH/watch started on the same boot, BootOrder 0005,0000 and
DriverOrder 0000,0001, absent BootNext, all six protected hashes unchanged,
`companion_pending=0`, and vendor configuration 1 still active. A verification
command also requested BootOrder deduplication; its observed value remained
0005,0000. No physical reboot, firmware code patch or native deployment occurred
in this batch.

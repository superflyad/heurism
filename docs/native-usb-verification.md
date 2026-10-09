# Native USB controller proof

The native kernel now owns a QEMU xHCI controller after ExitBootServices and
reads directly attached USB device/configuration descriptors. This is the first
USB transport layer needed for the Dell's TP-Link 2357:0601 Ethernet adapter.
The separate [USB Ethernet profile](native-usb-network-verification.md) now
configures bulk endpoints and runs a CDC ECM fixture. The Realtek protocol
remains unimplemented.
The Dell retains its working Linux management/default boot.

## Implemented

`drivers/usb/xhci.c` initializes an exclusively owned, mapped controller through
register/DMA callbacks. It validates register bounds and capabilities, halts and
resets the controller, checks 4-KiB page support, allocates command/event rings,
DCBAA, contexts, control-transfer buffers and up to 32 scratchpads. Bus mastering
begins after owned structures are initialized. ERST programming can immediately
fetch DMA memory, so DMA must be enabled before that register is published.

Commands and EP0 requests are serialized with cycle ownership and completion
pointer/type/slot checks. Each register/event wait has a hard poll bound. A
control request publishes its Setup TRB after the Data/Status TRBs. Root-port
reset, Enable Slot, Address Device, full-speed EP0 Evaluate Context,
GET_DESCRIPTOR and Disable Slot complete through native rings. Short/error or
unrelated completions fail inspection. Halting precedes DMA release; an uncertain
halt retains allocations and disables bus mastering. Firmware-owned legacy
capabilities are rejected rather than overridden.

The controller wrapper accepts only QEMU 1b36:000d with its known 16-KiB MMIO
aperture, behind `--vm-usb --vm-diagnostics`. Production excludes this startup
path. This inspection profile halts after enumeration; the separate USB network
profile retains the controller for bulk traffic. No Intel controller ownership,
physical IOMMU setup, hub traversal or physical reset is proved.

The implementation uses the register/data structure definitions in Intel's
[xHCI 1.2b specification](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf).
QEMU's [controller implementation](https://github.com/qemu/qemu/blob/master/hw/usb/hcd-xhci.c)
was used to check emulator behavior. OVMF places 64-bit PCI BARs near the top of
the CPU's reported address space; see its [runtime configuration documentation](https://github.com/tianocore/edk2/blob/master/OvmfPkg/RUNTIME_CONFIG.md#platform-physical-address-space-bits).
The normal USB fixture uses 36 physical address bits within the kernel's current
64-GiB identity map. A separate 40-bit fixture verifies rejection of the higher
BAR while native UI/timer progress continues. This is an explicit mapping limit,
not a claim that arbitrary high PCI BARs are supported.

## Reproduce

```powershell
python tools/build-kernel.py --vm-diagnostics --vm-usb --out build/native-usb
python tests/native_host.py --build build/native-usb
python tests/usb_host.py
python tests/native_usb_smoke.py
python tests/native_usb_smoke.py --case empty
python tests/native_usb_smoke.py --case absent
python tests/native_usb_smoke.py --case highbar
python tests/native_acceptance.py
```

The full acceptance run now has thirteen VM cases. It checks deterministic normal,
network, USB and USB network binaries and excludes fixture startup markers from production.
The USB build guard also rejects `--vm-usb` without diagnostics under Python `-O`.

| USB VM case | Evidence |
| --- | --- |
| Devices | Three real emulated devices; 12 native EP0 descriptor reads; 160 No-Op commands plus enumeration commands; command/event ring wraparound; DMA page count restored |
| Empty | Same initialized rings and 160 completions without attached USB devices; all owned pages released |
| Absent | No supported controller; normal native progress continues |
| High BAR | Controller outside mapped address range rejected; normal native progress continues |

The device fixture reads these values through the native kernel, then compares
them with fixture expectations and QMP's independent USB device inventory:

| Device | VID:PID | Speed | EP0 packet | Configuration bytes | Interfaces |
| --- | --- | --- | --- | --- | --- |
| QEMU keyboard | 0627:0001 | High | 64 | 34 | 1 |
| QEMU USB network | 0525:a4a2 | Full | 64 | 67 | 2 |
| QEMU storage | 46f4:0001 | SuperSpeed | 512 | 44 | 1 |

Host fixtures execute the actual C driver with low-address DMA memory. They
exercise ring cycles, scratchpad allocation, reset/run/readiness timeouts,
allocation failures, stale/wrong/error completions, EP0 timeout/short/error
transfers, malformed descriptors and retention on failed halt. Descriptor tests
reject 2,295 truncated/malformed/arbitrary inputs, including captured SuperSpeed CDC ECM inputs. These are bounded host models;
they complement rather than replace the actual QEMU descriptor transfers.

Results are `build/native-usb/usb-host-verification.json` and each
`vm-usb-*/verification.json`, `serial.log`, `qmp-usb.txt`. Reports bind the native
binaries and trace to SHA256. The aggregate is `build/native/acceptance.json`.

The final read-only Dell check is `build/native-usb/dell-management-after.txt`:
trusted root boot 7c465eeb-59ca-46c5-8f6a-31954e668b8c, SSH/watch started,
BootOrder 0005,0000, DriverOrder 0000,0001, no BootNext, six protected file hashes
unchanged and `companion_pending=0`. No Dell reboot or native deployment occurred
in this USB implementation batch.

Native bulk/CDC ECM traffic and reboot now have separate VM evidence. Next,
implement and validate the actual
Realtek adapter's protocol, production credentials and Dell recovery before
selecting a physical native boot. Enumeration of a QEMU NIC is not Dell network
control, and the existing management boot remains SSD-dependent.

# Native kernel verification, 2026-09-27

The native path consists of Companion's own EFI application and freestanding
x86-64 ELF kernel. QEMU/OVMF successfully transfers ownership with
ExitBootServices and the kernel draws Companion pixels, advances its own timer
and inventories the virtual platform's ACPI/PCI devices. It accepts native
keyboard input and draws typed text. Neither Linux nor GRUB is
loaded in these VM runs. This is an implemented bootstrap kernel, not yet a
replacement for the Dell's management system.

## Implementation and boundaries

The loader checks ELF architecture, segment bounds, page overlap, permissions,
alignment and entry point before allocating and loading the fixed 32 MiB image.
The versioned 528-byte BootInfo carries framebuffer data, the final EFI memory
map, reserved ranges, ACPI RSDP and stack. An assembly bridge changes from the
EFI Microsoft x64 calling convention to System V AMD64. The ELF loader has no
dynamic interpreter or runtime imports. Validation does not authenticate the
kernel image cryptographically.

Final handoff uses a preallocated 256 KiB memory-map buffer. An invalid map key
can trigger a fresh capture and retry, up to three attempts; no console, disk,
protocol or free calls occur between attempts. After any ExitBootServices
attempt, an unrecoverable error halts rather than attempting a general firmware
chainload. This follows the restrictions in the
[UEFI handoff specification](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html#efi-boot-services-exitbootservices).

The kernel installs its own GDT, IDT and TSS, with an alternate double-fault
stack. It replaces firmware page tables with a supervisor identity map below
64 GiB. Kernel text is read-only executable; read-only data is non-executable;
writable data is non-executable. CR0 write protection and EFER NX are enabled.
This is a bootstrap address space, not userspace isolation or a complete MMIO
cache-policy implementation for arbitrary devices.

The page allocator takes only conventional, non-runtime EFI RAM below 4 GiB,
excluding the first MiB and handoff reservations. A local APIC timer calibrated
against PIT channel 2 supplies approximately 100 Hz interrupts; idle uses HLT.
Only the xAPIC path has VM execution evidence. x2APIC code and physical Dell
timer operation remain separately unverified.

ACPI discovery validates RSDP and XSDT checksums and every referenced table's
length/checksum before collecting signatures. Reads are restricted to ACPI
reclaim/NVS descriptors in the final memory map. Table counts, sizes and total
checksum work are bounded. MADT records provide processor/IOAPIC counts and
MCFG records provide ECAM regions for later PCI discovery. Malformed data
discards the inventory. This does not execute AML, configure PCI, program an
IOAPIC, power off the system or establish native device drivers. Layout sources:
[ACPI programming model](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html)
and [EDK II MCFG definitions](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h).

PCI discovery reads vendor/device/class, header and assigned BAR values across
validated ECAM bus ranges, including nonzero first-bus offsets. It performs no
configuration writes, BAR sizing or bus-master enabling. Before access, the
kernel marks device windows uncached/non-executable and flushes cache lines
and translations. The bootstrap mapping uses 2 MiB envelopes, rejecting any
overlap with RAM, ACPI/runtime/loader memory or reserved handoff buffers.
Windows above 64 GiB remain unsupported. Address references:
[Linux ECAM definitions](https://github.com/torvalds/linux/blob/master/include/linux/pci-ecam.h)
and [x86 MCFG mapping](https://github.com/torvalds/linux/blob/master/arch/x86/pci/mmconfig_64.c).

The input driver uses i8042 ports 0x60/0x64 with translated set-1 scan codes.
Initialization polls within fixed bounds and permits at most three enable
retries. Controller IRQs are disabled for this polling path; the native timer
wakes the kernel to drain input. Events include press/release, extended codes
and US-layout characters. Shift, Caps Lock and Backspace work; Enter/Escape
clear the development typing line. This is not a shell, touchpad driver,
international layout or keyboard LED implementation. Protocol references:
[i8042 definitions](https://github.com/torvalds/linux/blob/master/include/linux/i8042.h)
and [QEMU PS/2](https://github.com/qemu/qemu/blob/master/hw/input/ps2.c).

Input is an explicit platform choice: diagnostic builds default to q35 i8042;
`--keyboard none` disables it. Production builds default to no port probing;
`--keyboard i8042` opts into that controller. The Dell's Linux inventory confirms
its keyboard is `isa0060/serio0/input0`, but the native driver has not run there.
Its FADT boot flags are 0x0001, omitting the i8042 flag despite the working
controller; do not infer absence from that flag alone. Read-only reference
evidence is `artifacts/hardware/native-input-reference.txt`.

## Evidence and reproduction

Run `python tests/native_acceptance.py` from the repository root. It builds
isolated diagnostic images, exercises actual C code in host fixtures, runs thirteen
VM cases, then rebuilds the normal image and compares binary hashes.

| Case | Observation |
| --- | --- |
| Normal | Own handoff/memory/timer/ACPI; PCI exactly matches QMP; real emulated key presses/releases produce typed framebuffer pixels |
| Missing kernel | Rejection before ExitBootServices; no kernel entry |
| Corrupt ELF signature | Rejection before ExitBootServices; no kernel entry |
| Stale map key | Second capture/attempt succeeds and kernel advances |
| Deliberate UD2 | Kernel reports exception vector 6 and stops before progress |
| Write to protected text | Kernel reports page fault vector 14, error 3 and address 0x02000000 |
| Input disabled | Timer/PCI still run; no keyboard initialization/ready marker |
| Native network | Real emulated NIC/ARP/ICMP/authenticated UDP; kernel reboot/reconnect; old-boot commands rejected |
| USB devices | Native xHCI command/event rings and EP0 device/configuration reads at full/high/SuperSpeed; DMA released after halt |
| Empty USB controller | Rings initialize/wrap and release without attached devices |
| USB controller absent | Normal kernel progress without probing an unsupported controller |
| USB BAR outside mapping | MMIO rejected; normal kernel progress preserved |
| Native USB Ethernet | Owned xHCI bulk/CDC ECM; 140 packet round trips; authenticated reboot/reconnect and fresh nonce; oversized frames rejected |

Host fixtures reject 20 malformed ELF variants, exercise nine final-handoff
cases, verify allocation/exhaustion/reservation/double-free invariants and check
ACPI checksums, truncated/invalid records, unreadable pointers, duplicate table
pointers and physical-reader coverage. PE checks establish EFI architecture,
executable entry, relocations and absent imports. Framebuffer tests compare the
actual rendered Companion title against pixels from the actual C renderer.
Additional host fixtures cover ECAM offsets, multifunction devices, raw BARs,
read failure, inventory exhaustion, overlapping/overflowing regions and device
mapping envelopes overlapping RAM/reservations. Controller fixtures cover
timeout/absent ports, ACK/resend/error, AUX/parity filtering and modifier,
extended and Pause decoding. Actual VM typed-text pixels are checked too.

Results are in `build/native/acceptance.json`, `host-verification.json` and each
`vm-*/verification.json`, with serial logs and screenshots next to them. The
normal VM discovers FACP, APIC, HPET, MCFG, WAET and BGRT, one enabled processor,
one IOAPIC and one ECAM region. These are virtual hardware observations.
Its five PCI devices are the q35 host bridge, standard VGA, LPC bridge, SATA and
SMBus. Every bus/slot/function, vendor/device and class tuple is compared against
QMP `query-pci`, saved as `qmp-pci.json`.

The measured normal run reached its second progress marker about 2.5 seconds
after QEMU launch. This includes OVMF and TCG startup and is not a Dell boot-time
measurement. No physical native boot or native network access is claimed.
`boot_marker_seconds` separates that initial observation from total test time,
which includes keyboard injection and screenshot verification.

## Physical deployment status

Only the hidden-menu presentation change has been deployed to the Dell. Its
management and rescue EFI binaries, Linux kernel and initramfs remain unchanged.
Root SSH returned after normal reboot. Native binaries remain local VM builds.

Native PCI/input, isolated e1000 network/management, xHCI inspection and USB ECM networking have VM evidence.
See [native networking](native-network-verification.md) and
[native USB](native-usb-verification.md) and
[USB networking](native-usb-network-verification.md). Physical validation,
Realtek Ethernet and production management credentials remain pending.
Intel VMD-owned NVMe devices require a separate ownership path; ordinary ECAM
scanning does not expose that private domain. Native kernel
execution cannot retain Linux SSH. Neither a firmware error screen nor a total
native hang can currently be controlled independently over Ethernet. Preserve
the default SSD management/recovery path and do not select unattended native
Dell boots until recovery/control evidence supports them.

# Optional native kernel research plan

Linux is the retained product kernel. This document describes a separate
freestanding kernel experiment, not the Companion product roadmap or a planned
Dell deployment. Its VM work is useful research; unfinished physical drivers do
not block the Linux-based OS. See [the product roadmap](roadmap.md).
Dell UEFI and embedded controllers remain underneath the installed Linux system.

## Installed startup and optional research branch

```text
Dell initialization
  -> Companion startup extension
  -> Companion loader and display
       -> stable Linux management and Companion services (product default)
       -> internal rescue (failed management boot)
```

The native kernel has a separate VM loader; it is not installed or selected on
the physical Dell. The remaining milestones below belong only to that lab.

The initial Dell logo belongs to the firmware phase. Our current display begins
after its handoff. Startup branding does not establish firmware replacement.
The extension bootstrap, management loader and rescue remain SSD-dependent.

The installed presentation keeps the proven recovery-aware GRUB loader,
with a hidden one-second Escape-key window and the stable/rescue journal. It adds
Companion colors, an explicit startup label and a concise local ready screen.
It suppresses routine Linux console messages, retains errors and leaves logs
available remotely. A continuous graphical splash is a later loader/UI task;
this iteration does not promise uninterrupted pixels through a display mode
change or firmware initialization.

## First kernel milestone

Build a freestanding x86-64 kernel that draws `COMPANION KERNEL` and a changing
counter using the inherited framebuffer **after ExitBootServices succeeds**.
Produce serial evidence in QEMU, and never use EFI console/boot-service calls
from kernel code. That observation separates a kernel from the existing EFI
screen experiment.

Use a small ELF64 kernel loaded by a separate UEFI application. Keep the current
startup extension as a separate component. Compile the loader with the Microsoft
x64 EFI ABI and the kernel with the System V AMD64 ABI; an explicit assembly
entry bridge sets the stack and passes the boot-info pointer. Use freestanding
C and small assembly routines, no red zone, no implicit runtime library, and
disable SIMD-dependent generated code until its CPU state is explicitly owned.
Use a fixed linked virtual layout first; defer a higher-half address-space
transition until the initial handoff is demonstrated.

The loader validates ELF headers, architecture, segment bounds, alignment,
overlaps, destination ranges and the executable entry point before loading.
Allocate segment and stack memory through EFI. Do not treat unvalidated file
offsets or ELF addresses as writable memory. Start with one CPU and interrupts
disabled, then install the kernel's own exception environment before enabling
interrupts. Halt visibly on a fatal error rather than silently returning into
firmware after ownership has transferred.

## Loader/kernel contract

`boot/` owns EFI types. A versioned `kernel/boot_info.h` contract should carry
fixed-width fields, size, version and reserved fields, with no EFI protocol
objects. The initial contract includes:

- Framebuffer physical base, byte extent, width, height, row stride and pixel
  format; validate all bounds before drawing. Support observed RGB/BGR layouts.
- Final memory-map buffer, byte size, descriptor stride and version. Preserve
  firmware-reserved and runtime regions; do not assume descriptors have a fixed
  size or reclaim loader buffers before the kernel copies their contents.
- Kernel segment ranges, stack range and all handoff buffers as reserved ranges.
- Validated ACPI RSDP address and the table revision; copy/validate ACPI headers
  and checksums before interpreting subsequent tables.
- Optional diagnostic output selected by platform: emulated serial first. Do
  not assume the physical laptop exposes a usable 16550 UART.

The loader obtains GOP and ACPI data and allocates handoff buffers before the
final memory-map capture. After the first ExitBootServices attempt, recovery
must follow the specification's restricted retry rules: refresh the map/key
without unrelated EFI calls, retry within a fixed bound, and do not invoke a
general chainload fallback after boot services may be partially shut down.
On success the kernel owns the next action. See the
[UEFI boot-services specification](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html#efi-boot-services-exitbootservices).
Pre-handoff validation failures can still return to the normal management loader.

## Implementation sequence and proof

| Step | Work | Required evidence |
| --- | --- | --- |
| K0 | Reproducible ELF kernel, EFI loader, ABI bridge and contract | Valid binaries; invalid/truncated/overlapping ELF files rejected; no OS imports |
| K1 | Final memory map and ExitBootServices | OVMF boot draws through kernel code; stale-key retry and exhausted retry fixtures; no post-exit EFI boot calls |
| K2 | Private GDT/IDT, exception reporting, physical page allocator | Deliberate exception has a useful report; reserved ranges excluded; exhaustion handled |
| K3 | Own page tables, local APIC/timer and interrupt dispatch | Controlled page fault; timer advances without firmware; serial trace survives exceptions |
| K4 | ACPI/PCI inventory and first native input | Match saved Dell identifiers; keyboard events through an identified native controller |
| K5 | Native management transport | Authenticated command/status/reboot channel and reconnect proof in VM, then physical hardware |
| K6 | Storage, processes and services | Read-only NVMe first; userspace isolation; service supervision and tested recovery |
| K7 | Interactive Companion UI and power | UI uses native input/display APIs; measured thermal, battery and suspend behavior |

K5 may precede full storage/userspace because management access is a priority.
The physical management link uses a Realtek USB Ethernet adapter: reproducing it
requires USB host-controller support, enumeration, the adapter protocol and a
network stack. Its [captured second configuration](dell-usb-ecm-investigation.md)
offers CDC ECM; investigate that standard path before implementing vendor mode.
Physical ECM traffic is not yet proved. UEFI networking stops being a substitute once boot services end.
A virtual NIC is the first network development target, not proof of the Dell
USB driver's behavior. Native Wi-Fi, accelerated Intel graphics and audio come
after the basic kernel and management path; each has its own device milestone.

## Recovery and deployment rule

Native development never becomes the default just because it boots once. Keep
the existing management and recovery EFI files, startup driver order and default
boot entries intact. VM iterations come first. Add native selection only after
the loader has a tested failure path and the kernel's recovery behavior is
understood; never use native Dell PXE as a shortcut.

While native kernel code runs, Linux root SSH is unavailable. Installing a
kernel alongside management does not create concurrent access or an independent
reset channel. A preboot EFI fallback cannot recover a kernel hang after
ExitBootServices. The failed physical TCO reset experiment remains relevant.
Before unattended physical kernel runs, demonstrate a bounded reset that works
on this board, or provide another validated independent recovery channel.
Owner-attended experiments can be considered separately, with their access
gap stated explicitly.

Until K5 is physically demonstrated, the reliable control surface remains the
Linux management installation. Native boots, failures and recovery are recorded
separately from host and VM passes. See [autonomous recovery](autonomous-recovery.md)
and [hardware targets](hardware-targets.md).

## Implemented and next work

K0 through K3 are implemented with host/VM evidence. The separate EFI loader,
ELF kernel, ABI bridge and platform-neutral renderer pass normal and failure
fixtures, including stale-key retry, deliberate UD2 and protected-text page
faults. K4 includes validated ACPI inventory, read-only PCI ECAM scanning matched
against QMP and native i8042 typing in VM. Physical controller validation remains
pending. See [native verification](native-kernel-verification.md)
for commands, evidence and limits. No native kernel is installed or selected
on the physical Dell.

K5 has an isolated e1000/UDP fixture: authenticated status, kernel reboot and
reconnect pass in VM; captured old-boot commands are rejected. Its public test
key is excluded from production. See [native network proof](native-network-verification.md).

The first xHCI/USB transport step now enumerates root-port devices in an isolated
VM, including full/high/SuperSpeed EP0 requests, bounded command/event rings and
owned DMA cleanup. See [native USB proof](native-usb-verification.md).

Retained USB devices, bulk endpoints and CDC ECM traffic now pass the isolated
[USB network fixture](native-usb-network-verification.md), including authenticated
kernel reboot and reconnect. This is not the Dell's Realtek protocol.

The actual adapter's second ECM configuration now passes native descriptor and
setup host models at SuperSpeed, including both xHCI context strides. See
[Dell ECM investigation](dell-usb-ecm-investigation.md).

The measured Intel BAR is above the current 64-GiB mapping limit and has a
64-KiB aperture. Next: bounded sparse high MMIO/framebuffer mappings, physical
Intel xHCI ownership/IOMMU work, physical ECM traffic (or Realtek
vendor mode if needed), production credentials
and a Dell-verified recovery/reset path. Match
physical identifiers against the saved Dell audit before device ownership.
Keep VM and physical claims separate and continue preserving root access.

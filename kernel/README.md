# Native Companion kernel

Optional research only. The Companion product retains Linux as its kernel and
hardware foundation. This kernel is not installed on the Dell and its physical
driver gaps are not product prerequisites. Recorded verification applies to the
source hashes in each report; the latest sparse mapping edits have only a build
and empty-USB VM smoke check, not the complete acceptance suite.

Freestanding x86-64 ELF kernel with a separate Companion EFI loader. The kernel
executes after successful ExitBootServices, draws its own framebuffer screen,
owns GDT/IDT/TSS and page tables, allocates physical pages and sleeps between
local APIC timer interrupts. It validates and inventories ACPI tables without
executing AML, scans PCI without configuration writes and handles native i8042
keyboard input. There is no Linux or GRUB in this native VM path.

From the repository root on Windows with Visual Studio's bundled LLVM:

```powershell
python tools/build-kernel.py --vm-diagnostics
python tests/native_host.py
python tests/native_smoke.py
```

The smoke test uses the existing QEMU runtime under `build/tools/qemu`.
For all twelve VM cases and deterministic rebuild verification:

```powershell
python tests/native_acceptance.py
```

Output is isolated in `build/native`: `kernel.elf`, `companion-loader.efi`, an
ESP tree, binary hashes, host results and VM screenshots/serial traces.
`--vm-diagnostics` enables emulated COM1; these builds are VM-only. Omit it
for a build with no assumed physical UART. Diagnostic builds default to i8042
input; use --keyboard none to disable it. Production builds default to no input
port probing; --keyboard i8042 is an explicit platform choice.
Neither command installs on the
Dell or changes its default boot.

The kernel currently has no physical driver validation, device storage, production network
management, processes or independent recovery. Linux SSH stops when a native
kernel takes ownership. Keep the working Dell management path while building
these capabilities. See [verification](../docs/native-kernel-verification.md)
and [remaining milestones](../docs/kernel-plan.md).

The explicit --vm-network profile adds an isolated e1000/UDP management fixture
with authenticated status and reboot/reconnect tests. Its key is public fixture
data and production excludes the endpoint. See ../docs/native-network-verification.md.

The explicit --vm-usb profile adds QEMU xHCI control transfers and descriptor
inspection, with bounded polling and owned DMA cleanup. It is compiled out of
production. The additional --vm-usb-network profile configures bulk endpoints
and tests native CDC ECM management/reboot. See
[native USB proof](../docs/native-usb-verification.md)
and [USB network proof](../docs/native-usb-network-verification.md).

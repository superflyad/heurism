# Heurism

Heurism is our Linux distribution for the Dell and its development VM. It uses
upstream Linux, Alpine packages and Unix tools while Heurism owns the system
identity, VM package selection, startup integration, C shell and terminal, control
service, desktop integration and release checks. Xfce currently supplies the
established desktop session. Older installed boot and management interfaces
retain their previous identifiers during migration. See the
[system base record](docs/heurism-system-base.md) for what is owned today and
what still comes from upstream.

Development runs first in the [PrimeServer Hyper-V VM](docs/prime-vm.md),
`CompanionDev`, with separate root SSH, versioned UI releases, host reset and
checkpoints. Guarded releases also run on the physical Dell. Linux remains the
kernel and hardware foundation on both targets. The active Dell desktop uses
Alpine's established Xfce session for its panel, workspace, window management,
file manager and editor. The [native C runtime](docs/native-runtime.md) provides
the Heurism shell, graphical terminal, hardware settings, local
root control service and sealed release verifier. The original C workspace is
selectable for recovery and the previous Python/Tk desktop remains sealed.
See the [Xfce bridge](docs/xfce-bridge.md) for integration details and the
[security status](docs/security.md) for verified protections and remaining limits.

On the Dell, use the red power icon at the top right for Restart or Shut down.
Choose an action, then click its confirmation button within ten seconds. This
uses Heurism's checked C power control. The old Xfce Log Out dialog had
disabled power actions here; Applications > System > Heurism Power is now
the working menu route.

The active sealed C releases are selected by `/opt/heurism/native/current` on
both machines. See the [Heurism migration record](docs/heurism-migration.md)
for release IDs, checks and the older boot and recovery identifiers that remain
in place.

The [provisioning USB](docs/provisioning-usb.md) now provides the path to root
remote access, hardware auditing and a persistent management installation on
the Dell. The installed kernel and drivers currently come from Alpine packages.
The UEFI framebuffer experiment below remains available in its boot menu.

The Dell's 512 GB SSD now contains the management system. Physical SSD boot
without USB and trusted root SSH are verified. See the [deployment record](docs/dell-deployment.md)
and [reconnect/recovery commands](docs/remote-control.md).
An [internal rescue system and crash recovery](docs/autonomous-recovery.md)
now provide recovery without the USB or the main root filesystem.
See [firmware access and connection hardening](docs/access-progress.md) for
remote BIOS tools, the Boot Guard findings and autonomous broken-SSH boot recovery.
See [motherboard storage and automatic startup](docs/nv-dispatch-research.md)
for the verified NVRAM payload, Dell loader tests, and remaining bootstrap gap.
The [automatic NVRAM startup driver](docs/nv-startup.md) now executes that
payload during normal Dell startup; a physical reboot returned healthy root
SSH. Its initial loader still depends on the SSD and includes an SSD fallback.

A personal computer platform we can understand and control, using the Dell
Inspiron 7506 2-in-1 as practice hardware (target 0). The aim is ownership of
startup policy, system services, hardware controls, applications and interface.
Linux supplies the kernel and device drivers. We are building and releasing the
Heurism operating system around that foundation. See the [architecture](docs/architecture.md)
and [product roadmap](docs/roadmap.md).

The installed [startup presentation](docs/startup-presentation.md) hides GRUB
during normal management boot; Escape during the one-second window reveals
recovery choices. Root management returned after the physical reboot.

The historical [Companion desktop 0.3](docs/desktop.md) was installed on Linux. Version 0.3 added
managed application windows, Files with recoverable Trash, an editor with draft
recovery, Firefox, user and Administrator terminals, an on-screen keyboard,
Wi-Fi configuration, speaker controls, supported BIOS attributes and deliberate
restart/shutdown. Saved libinput settings handle the touchpad; the touchscreen
uses pointer emulation. Root management remains independent of the UI.
See [workspace workflows and verification](docs/workspace.md) for acceptance
evidence, versioned releases, automatic failed-UI fallback and remaining limits.

## Optional kernel research

These experiments are retained as a learning track, not prerequisites for the
Heurism product or a planned replacement for the Dell's Linux installation.
The [native kernel](docs/native-kernel-verification.md) boots through our own
EFI loader in QEMU, exits firmware boot services, draws Companion and owns
exceptions, paging, page allocation and timer interrupts. It also validates ACPI
inventory, PCI discovery and i8042 typing. This native path loads neither Linux
nor GRUB. Native Ethernet and other device drivers
and authenticated management have isolated VM fixtures. The Dell uses the
proven Linux management default. Historical evidence is tied to recorded source
hashes; subsequent experimental edits need fresh validation. Build and test with:

```powershell
python tests/native_acceptance.py
```

See [kernel milestones](docs/kernel-plan.md) for remaining work and recovery gates.

An [isolated native network fixture](docs/native-network-verification.md) now
passes emulated Ethernet, authenticated status and kernel reboot/reconnect.
Its public test key is excluded from production. No kernel switch is planned.

The [native USB transport](docs/native-usb-verification.md) now owns an emulated
xHCI controller and reads root-port descriptors at full/high/SuperSpeed.
[Native USB Ethernet](docs/native-usb-network-verification.md) now passes bulk
traffic and authenticated reboot/reconnect in a CDC ECM VM fixture. Physical
Intel ownership and physical Ethernet traffic remain unverified research items.
The Dell adapter's [captured CDC ECM configuration](docs/dell-usb-ecm-investigation.md)
now passes native descriptor/setup host models.

## First executable

Milestone 0 is a freestanding x64 UEFI application. It discovers the Graphics
Output Protocol, validates its framebuffer, and draws our own Companion screen.
It shows an initial `COMPANION BOOT 0.2 - EFI ENTRY REACHED` diagnostic banner,
prefers the firmware console display and falls back to text mode if graphics
are unsupported. Fatal errors remain visible for 15 seconds.
It runs without Windows or Linux underneath it. **UEFI boot services remain
active; this is not yet a kernel.** Press Escape to return to the firmware boot
manager. No disk writes or firmware-variable writes are performed by the app.

On Windows with LLVM (`clang` and `lld-link`) and Python:

```powershell
.\tools\build.ps1 -Test
```

The script searches PATH and Visual Studio's bundled x64 LLVM tools, or accepts
explicit `-Clang` and `-Linker` paths. Output:
`build/esp/EFI/BOOT/BOOTX64.EFI`. A hash is printed for recording each experiment.
Host tests also generate `build/companion-screen.png` from the actual C renderer.
Source is C11 with a small, documented UEFI ABI subset; no SDK or runtime library
is linked. Only x86-64 is implemented at this stage.

See [the verification record](docs/verification.md) for completed checks and
the remaining emulator/hardware work.

Read [the boot procedure](docs/boot-usb.md) before trying it on the Dell.
For an emulator, provide QEMU and a combined x64 OVMF image:

```powershell
.\tools\run-qemu.ps1 -Firmware C:\firmware\OVMF.fd
```

These tools are not bundled. Emulator testing is a separate step from host tests.
An automated virtual USB test is also available:

```powershell
python .\tests\uefi_smoke.py C:\path\to\qemu
```

It expects QEMU's bundled EDK II code/vars firmware under `share/`, checks the
actual rendered title and sends Escape to verify return to firmware. Firmware
screenshots are saved under `build/uefi-smoke/`.

## Project map

A [Companion UEFI extension](docs/firmware-extension.md) is now registered
alongside Dell's firmware. A separate preboot observer verified that Dell loaded
the owner driver on two physical boots and that healthy root management returned.
Its automatic driver copy remains on the SSD's EFI partition. General preboot
remote management and SSD-independent startup are not yet implemented.

The [NVRAM payload experiment](docs/nv-extension-research.md) tests a narrower
board-storage path: a 2 KiB owner image stored through UEFI variable services
and explicitly loaded from retrieved bytes by a preboot bootstrap. This does
not make the bootstrap independent of the SSD.

[Preboot network tests](docs/preboot-network-testing.md) now prove Ethernet
delivery and execution of the pinned owner extension before Linux starts,
rejection of an altered copy, and healthy root return with the test controller
offline. The initial bootstrap and fallback still depend on the SSD.

The [network RAM management image](docs/network-ram-management.md) passes
firmware PXE boot and pinned root SSH in a diskless VM, including recovery
after SSH/DHCP termination. The physical Dell reached PXE DHCP but did not
download the probe or restore SSH; the owner's photo confirms it stopped at
Dell SupportAssist with "No bootable devices found". Physical PXE tests are blocked.
Root SSH subsequently returned after the owner cleared the halt; SSD default
boot and Companion driver order are restored and the current boot is healthy.
Physical SSD-independent management remains unproven.

| Directory | Purpose |
| --- | --- |
| `boot/` | UEFI entry point and the firmware ABI |
| `kernel/` | Freestanding kernel, handoff contract and page allocator |
| `platform/x86_64/` | CPU entry, paging, exceptions, timer and ACPI inventory |
| `platform/arm64/` | Future port boundary; no implementation yet |
| `drivers/` | Future hardware drivers |
| `userspace/` | Future services and applications |
| `ui/` | Experimental framebuffer renderer; future Heurism research interface |
| `tools/` | Build, inventory and emulator scripts |
| `tests/` | Actual PE image checks and C tests with mock firmware |
| `docs/` | Architecture, hardware facts, milestones and evidence |

Start with [architecture](docs/architecture.md),
[hardware targets](docs/hardware-targets.md), and [roadmap](docs/roadmap.md).
The working rule is **retrieve → verify → reason → challenge → plan → implement
→ test → prove**. A successful compilation is evidence of compilation, not
evidence that a physical machine booted it.

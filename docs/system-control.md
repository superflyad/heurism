# Companion and system control

Current owner direction: Linux stays. Companion builds the OS experience,
services and direct hardware interfaces on the installed Linux foundation.
Replacing Linux, Dell firmware or Intel components is not a product requirement.
Supported BIOS interfaces and reliable root management remain important.

The 16 GB USB served as provisioning media. The Dell now has a persistent Alpine
management installation on its SSD, internal rescue and authenticated root SSH.
Physical boots without USB and supported BIOS setting changes are recorded.
The Companion startup extension loads an NVRAM payload through an SSD bootstrap;
this does not establish SSD-independent startup.

Live management requires running target software and working communications.
It does not provide access while the laptop is halted in an arbitrary BIOS menu,
powered off or unable to boot. Root privilege does not remove firmware write
protections or signature requirements. Preserve the working management paths
while building the local interface and system control service.

## Control boundaries

| Layer | Intended control | Current evidence / boundary |
| --- | --- | --- |
| Boot selection | Companion as default, deliberate recovery path | SSD default boot, EFI boot journal and internal RAM rescue physically verified |
| UEFI bootloader | Own source, executable and kernel-loading policy | Companion EFI app implemented and photographed on Dell |
| Kernel execution | Retain Linux for memory, scheduling and device ownership | Linux runs on the Dell; custom kernel is optional VM research |
| Hardware drivers | Use Linux drivers through supported device/control interfaces | Linux supplies the installed hardware stack; product feature acceptance remains |
| Services and UI | Own application/service model and interaction | First Linux local shell/API installed; live state, brightness, saved appearance and UI recovery verified |
| BIOS/UEFI settings | Configure exposed startup, security and device options | 90 exposed attributes inventoried; remote enum settings tool installed; warning/power settings persisted |
| BIOS/UEFI implementation | Retain Dell firmware; supported settings and extension only | No supported replacement or board-specific recovery plan established for target 0 |
| EC / Intel management / device firmware / microcode | Inventory ownership and limitations separately | Not replaced or controlled by Companion |

The product execution target is:

```text
Power on -> Dell initialization -> Companion extension and recovery loader
  -> Linux kernel/drivers -> Companion services and local interface
```

Persistent boot selection and root reconnection are already recorded. Further
product work should verify the interface and control service without disabling
these paths. The custom kernel's separate VM evidence is optional research.

## Firmware research track

Dell documents UEFI-only boot, Secure Boot, custom key management, USB boot
support and recovery settings for the 7506. Those are configuration controls,
not documentation for replacing the entire firmware. Exact variant and firmware
revision are now inventoried (Dell 0VK62X, BIOS 1.35.0). Supported BIOS settings
were read and changed through the Linux Dell WMI interface; this is configuration
of Dell firmware rather than replacement of its implementation.

coreboot's FAQ says a port requires supported CPU/chipset hardware and checking
whether proprietary firmware is enforced. Intel Boot Guard can make replacement
difficult or impossible. Read-only MSR 0x13A reads now report **measured and
verified boot enabled** on all eight logical CPUs (`0x000000030000007f`), decoded
using coreboot's CBnT definitions. This is evidence of active firmware verification,
not a complete flash/fuse/policy audit. See [access progress](access-progress.md).
Disabling UEFI
Secure Boot for our OS does not establish that Boot Guard is disabled or that
arbitrary motherboard firmware is accepted. A verified board port, identified
flash devices and independent recovery method must precede any firmware write.

The Dell remains target 0. Firmware replacement is not a product requirement.
The [physical firmware ownership audit](firmware-ownership.md) retains earlier
research evidence and unresolved constraints; its replacement objectives are
historical and superseded by the Linux-based product direction.

## Next control milestones

1. Extend the installed status/brightness/appearance API with supported settings
   and BIOS tools, beginning with read-only BIOS attribute views.
2. Improve the installed local interface and verify physical input workflows.
3. Add verified service actions and update rollback while preserving root SSH and recovery.
4. Measure supported reboot, power and suspend behavior on the Dell. Report any
   firmware pauses or recovery limits rather than claiming continuous access.

Sources:

- [Dell 7506 system setup options](https://www.dell.com/support/manuals/en-us/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-black-service-manual/system-setup-options?guid=guid-cb19996e-6cf5-47b9-be58-1a039da03b99&lang=en-us)
- [UEFI boot services and ExitBootServices](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html)
- [coreboot porting and firmware constraints](https://doc.coreboot.org/getting_started/faq.html)

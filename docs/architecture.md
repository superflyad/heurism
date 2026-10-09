# Architecture and ownership

Companion is a Linux-based OS experience and hardware control platform. Target 0
is the Dell Inspiron 7506. Alpine Linux stays as the kernel, driver and base
system foundation. Companion owns the startup experience, service policy,
interface, applications and hardware control workflows. Rebuilding Linux or
its USB, networking, storage and graphics drivers is not the product goal.

## System boundaries

```text
Dell UEFI and embedded controllers
  -> existing Companion startup extension (SSD bootstrap + NVRAM payload)
  -> recovery-aware GRUB (hidden; Escape reveals recovery)
  -> Linux kernel and existing device drivers
  -> system services and authenticated remote management
  -> Companion control service and session services
  -> Companion local interface and applications
```

The installed system currently provides persistent Linux root management,
internal rescue, supported BIOS setting tools and a minimal startup presentation.
The [graphical desktop and local control service](desktop.md) are installed.
The [workspace](workspace.md) provides managed windows, documents and draft
recovery, a browser, terminals, an on-screen keyboard and network/audio/BIOS
settings. Fixed power actions verify the SSD recovery paths before proceeding.
Root management survives UI restart and failed-UI rollback.
The startup extension and management/recovery loaders still depend on the SSD.
See [startup presentation](startup-presentation.md),
[remote control](remote-control.md) and [system control](system-control.md).

## Direct hardware interfaces

Companion services should obtain real device state through Linux device nodes,
sysfs, supported ioctls and existing system services. Display and input should
use the Linux graphics and input stack. Network, battery, thermal and power
features should use the existing drivers and measured platform capabilities.
Supported Dell BIOS attributes remain available through the installed BIOS tool.
We do not need to write a USB controller driver to control an Ethernet adapter
that Linux already supports.

A privileged control service should expose specific operations and structured
state/events to the UI. Validate requests and report actual results. Run the UI
without root where practical; keep remote management and recovery separate from
its process lifecycle. A UI crash or restart must not remove management access.
The implemented local API uses a peer-checked Unix socket. Tk/Xorg/libinput
provide the interface and input handling; Openbox manages application windows.

## Control limits

Root access permits operating-system administration and supported hardware
interfaces. It does not grant arbitrary firmware writes or access while the
machine is stopped at a firmware error screen. Dell UEFI, Intel components and
embedded controllers remain in place. BIOS settings, NVRAM payload storage and
firmware replacement are distinct capabilities; report each separately.

Power, charging, sleep and thermals need physical observations before claiming
support. Preserve the known SSD boot and recovery paths while improving startup.
Current management is not an independent out-of-band reset channel.

## Optional research

The freestanding EFI app, custom kernel and native USB/network drivers remain
isolated learning experiments. Their host/VM evidence is recorded in the
[native kernel record](native-kernel-verification.md) and related proof documents.
They are not installed on the Dell and are not prerequisites for Companion.
See [the optional kernel plan](kernel-plan.md).

## Verification contract

Distinguish planned behavior, host/VM tests and physical Dell observations.
For product changes, demonstrate real state and successful actions, UI recovery,
and continued authenticated management. Verify protected boot/recovery files
before any reboot. Never infer working physical control from emulator results.

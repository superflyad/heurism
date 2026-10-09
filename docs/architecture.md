# Architecture and ownership

Heurism is our Linux distribution and hardware control platform. Target 0 is
the Dell Inspiron 7506. The installed kernel, drivers, package manager and many
desktop components currently come from Alpine upstream packages. Heurism owns
the distribution identity, VM package selection, startup experience, service
policy, C runtime, interface integration and hardware control workflows.
Rebuilding Linux or its USB, networking, storage and graphics drivers is not
the product goal. The [system base record](heurism-system-base.md) tracks the
remaining work to make the image and update lifecycle Heurism-owned.

## System boundaries

```text
Dell UEFI and embedded controllers
  -> existing SSD startup extension (installed before the Heurism rename)
  -> recovery-aware GRUB (hidden; Escape reveals recovery)
  -> Linux kernel and drivers from upstream packages
  -> system services and authenticated remote management
  -> Heurism C control service and upstream Xfce session
  -> Heurism local interface and applications
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

Heurism services obtain real device state through Linux device nodes,
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
The implemented local API uses a peer-checked Unix socket. Xorg/libinput
provide display and input handling; Xfce manages the default desktop. The
original C workspace uses Openbox when selected. Python/Tk is sealed recovery.

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
They are not installed on the Dell and are not prerequisites for Heurism.
See [the optional kernel plan](kernel-plan.md).

## Verification contract

Distinguish planned behavior, host/VM tests and physical Dell observations.
For product changes, demonstrate real state and successful actions, UI recovery,
and continued authenticated management. Verify protected boot/recovery files
before any reboot. Never infer working physical control from emulator results.

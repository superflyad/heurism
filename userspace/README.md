# Companion services and applications

Linux is the retained kernel and driver foundation. This directory is intended
for Companion's system control service and application/session services.
The local UI consumes a defined service API; privileged hardware actions stay in
the control service. Root SSH and recovery remain independent of UI availability.

`control.py` provides a bounded Unix-socket API for status, brightness, appearance,
libinput settings, wireless configuration, supported keyboard BIOS settings,
an explicit Administrator terminal and checked reboot/shutdown. `shell.py` and
`applications.py` provide the local desktop and document/settings workflows.
Openbox manages windows; Firefox, xterm, Onboard and PulseAudio supply existing
Linux applications and device interfaces. `release.py` verifies installs and
restores a previous working UI if a candidate fails initial startup. Root SSH
and the SSD boot/rescue system do not depend on this graphical session.
See [deployment](../docs/desktop.md), [workflow evidence](../docs/workspace.md)
and [the roadmap](../docs/roadmap.md).

The next Companion-owned runtime is being built in C under
[native](native/README.md). Its shell and graphical terminal are installed only
on CompanionDev for now. The current Python desktop/control code is a working
legacy layer during the migration, not the target implementation.

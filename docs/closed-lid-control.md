# Closed-lid control and local wake

On 2026-10-07, the Dell Inspiron 7506 2n1 was reachable by pinned root SSH
while its lid sensor read `closed`, AC was online, and `eth0` had
`10.8.22.238/24`. The management host was on the same subnet at
`10.8.22.122/24`. The running r8152 USB Ethernet adapter reported
`Supports Wake-on: pumbg` and `Wake-on: g`; its USB device and XHCI controller
both had wake enabled. Linux exposed only `[s2idle]` for memory sleep.

A C guarded reboot with the lid closed returned fresh boot
`fe284286-173f-4aa0-8b97-ddca43b80693`. Root SSH, watch, control, Xfce,
release verification, power checks and all six protected EFI/kernel hashes
passed. Dell firmware appended its known auto-created NIC boot entries; after
checking them, `BootOrder` was restored to `0005,0000`. `BootCurrent` was
`0005`, `DriverOrder` was `0000,0001`, and `BootNext` was absent.

Two physical suspends used `rtcwake -m mem -s 120` as a timed fallback. The
first entered `s2idle` at 00:16:38 UTC and resumed at 00:16:51 UTC after the
local host sent a magic packet; its RTC alarm was set for 00:18:39 UTC. The
second entered at 00:17:09 UTC. The host found port 22 unreachable after
25 seconds without a packet, then sent one packet with `tools/dell.py --wake`.
The Dell resumed at 00:17:44 UTC, about eight seconds after the packet and
well before its 00:19:10 UTC RTC alarm. Both resumes retained the same boot ID,
the lid remained closed, and kernel suspend stats reported two successes and
zero failures. The RTC alarm was cleared, `Wake-on: g` remained set, and SSH,
control, release health and protected boot checks passed.
The two guest timing logs were saved locally as
`artifacts/hardware/dell-wol-first-20261007.log` and
`artifacts/hardware/dell-wol-second-20261007.log`.

To wake a suspended Dell from the management host on this LAN:

```powershell
python tools/dell.py --wake --wait 180 --command '/opt/companion/native/current/companion-release health'
```

This establishes a closed-lid workflow for a running system and local magic
packet wake from Linux `s2idle`. It does not establish wake from full poweroff,
firmware halt, an unplugged adapter or a different network. Root SSH remains
independent of the desktop but is unavailable while the machine is asleep.

On 2026-10-08, the live Dell still reported the direct USB r8152 adapter at
`usb-0000:00:14.0-2`, `Wake-on: g`, AC online, and the same healthy boot ID.
The only RTC wake claim in its kernel log is from S4; there is no swap-backed
hibernation configured and no proven unattended S5 recovery. Dell's
[USB-Ethernet Wake-on-LAN article](https://www.dell.com/support/kbdoc/en-ap/000179586/wake-on-lan-does-not-work-over-a-usb-to-ethernet-adapter-connection?lang=en)
states that a direct USB-to-Ethernet adapter does not wake a powered-off (S5)
computer and calls for an onboard NIC or powered dock. The
[Inspiron 7506 firmware manual](https://www.dell.com/support/manuals/en-us/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-black-service-manual/system-setup-options?guid=guid-cb19996e-6cf5-47b9-be58-1a039da03b99&lang=en-us)
describes Wake on Dell USB-C Dock as a standby wake option; this system has no
powered dock on the management link. Full-shutdown magic-packet wake is thus
unsupported by the documented hardware arrangement and remains physically
untested. A power-off trial requires someone able to press the Dell's power
button if it does not wake.

# Dell control independence

The goal is to recover the Dell without requiring its owner at the keyboard.
This needs three distinct control layers. Each must be tested on the physical
machine before the next layer is described as working.

| Failure state | Working control today | Remaining gap |
| --- | --- | --- |
| Xfce or Heurism UI fails | Key-authenticated root SSH, OpenRC watch, sealed release rollback | Exercise more failed UI and update cases |
| Installed Linux fails after GRUB | Boot journal, rescue system, boot deadline, panic reset | Prove complete system update rollback and data restore |
| Firmware stops at Dell logo | None | An external reset and an independent observation path |
| Dell is fully off | Power button locally; Wake on AC setting is enabled | Remote cold power-on is unproven |

The 2026-10-09 incident crossed the third boundary. OpenRC shut down cleanly,
then the owner found the machine stuck at the Dell logo and recovered it with a
physical power cycle. One later monitored restart, after restoring Dell's
continue-on-warning setting, returned normally on boot
`9668135f-ca95-4be3-a0d9-78a6fc9a78a7`. That single pass does not make a
firmware halt recoverable. The current USB Ethernet adapter wakes Linux from
`s2idle`; it has not woken this machine from a firmware halt or full poweroff.
The Intel TCO watchdog did not reset the Dell in its physical test.

## Control path to build

1. Keep VM development and Dell release rollback under authenticated SSH while
   preserving the known SSD boot and rescue paths. Record the boot ID, release,
   power command and bounded return result for every physical restart.
2. Give the controller a view outside the installed OS: independent network
   power, a way to distinguish an operating system or Ethernet outage from a
   firmware stop, and a separately powered actuator for the physical power
   button. A mains switch alone is insufficient for a battery-backed laptop.
3. Require an explicit, bounded recovery state machine. It first checks the
   controller's own health, power/console evidence and pinned Dell identity;
   it never treats one missed SSH probe as proof of a firmware halt. It allows
   at most one recovery action per incident, records its evidence, and stops
   after a failed attempt rather than looping power cycles.
4. Test the state machine against simulated loss of SSH, Ethernet, Linux,
   bootloader and firmware progress before attaching an actuator. Then prove
   physical button control with the owner present, including a stopped-logo
   scenario if it occurs naturally. Only after repeated physical recoveries
   should an unattended restart be enabled.

The current Inspiron exposes a CSME HECI device and `/dev/mei0`, but neither
establishes usable Intel AMT. No AMT control appeared in its exposed Dell BIOS
attributes, and TCP port 16992 did not accept a local connection on the current
boot. This is an inventory result, not proof that the platform lacks AMT.
Do not enable firmware remote-management features or alter boot order based on
that inference.

**Acceptance for complete independence:** from a firmware-logo stall and from
a powered-off state, a separately powered controller can observe the condition,
restore the Dell to a fresh pinned SSH boot without local help, and produce an
audit record. Until that is physically demonstrated, Dell power operations
retain the `--local-recovery-ready` guard.

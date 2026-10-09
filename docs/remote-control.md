# Dell remote control

The management environment provides key-authenticated root SSH on the owner's
local network. The client private key stays in this workspace's ignored
`artifacts/ssh/`; the target's host identity is pinned in `known_hosts`.

For ordinary commands or reconnection after a reboot:

```powershell
python tools/dell.py --wait 180 --command 'id; findmnt /; heurism-status'
```

The tool first tries the last known address. If necessary, it scans TCP port 22
on the configured local /24, then requires the expected SSH host key, authorized
client key, root UID and target hostname before using a discovered address. It
stores the new address and boot ID under `artifacts/targets/dell.json`. Remote
commands execute once: the tool does not automatically retry a command that may
already have changed the system. `--timeout` sets the command's execution limit.
The installed hostname is `heurism-dell`; the helper also recognizes the old
hostname during rollback, but still requires the same pinned SSH host key and
authorized root key.
`--after-boot <previous-boot-id>` requires a changed boot ID before executing the
command, preventing a fast connection to the old system from falsely verifying
a scheduled reboot. SSH can become available before every startup service is
ready; explicitly check or wait for services when a command requires them.

`companion-watch` runs under OpenRC's daemon supervisor. Every ten seconds it
checks network interfaces, restarts missing DHCP clients, starts saved Wi-Fi
configuration when needed and restores a stopped/crashed SSH listener. Existing
SSH sessions disconnect during a reboot; reconnection establishes a new session.
The USB and installed-system tests deliberately stop DHCP and SSH to verify this.
The watcher also checks a completed SSH key exchange, so a frozen listener can
be recovered even when its PID still exists. An independent boot deadline resets
an unconfirmed installed boot into rescue after 120 seconds. See
[access progress and firmware controls](access-progress.md) for the tests and
the stronger checks on pinned identities before committing a boot.

The [internal rescue loader](autonomous-recovery.md) can boot a separate RAM
management system without the USB or the main root filesystem. Its boot journal
selects rescue after an unconfirmed previous boot. The Intel TCO watchdog failed
its physical reset test and is disabled; kernel panic/lockup recovery is configured.
The USB is another
recovery option from F12 with Secure Boot disabled. Local
root login does not require a password; remote password authentication is
disabled. Keep Ethernet connected to the same router. Wi-Fi requires a saved
configuration established with `companion-wifi`.

The actual USB Ethernet adapter reports Wake-on-LAN magic-packet support and
`Wake-on: g`. On 2026-10-07, two physical closed-lid `s2idle` suspends resumed
after a local magic packet, before their two-minute RTC alarms. The second
remained unreachable for 25 seconds before the packet and returned about eight
seconds afterward. A local wake request can be sent using:

```powershell
python tools/dell.py --wake --wait 180 --command 'heurism-status'
```

This verifies wake from Linux `s2idle` on the current AC/Ethernet setup. It does
not establish wake from full shutdown, a firmware error screen, or a lost USB
Ethernet connection. The sending host must be on the local network that accepts
the broadcast packet. See [closed-lid control](closed-lid-control.md).
Cold power-on, BIOS screens and lost networking do not provide an SSH control
channel. Reset and rescue can restore a new SSH session after supported failures;
SSH does not remain active during a kernel crash. Independent out-of-band
hardware would be needed for reliable control across all of those states.
Motherboard firmware is unchanged.

This Dell's supported BIOS settings are exposed under
`/sys/class/firmware-attributes/dell-wmi-sysman/attributes/`. Setting descriptions,
current values and allowed alternatives were inspected before changing adapter
warnings, warning behavior and Wake on AC. The values persisted after reboot;
see [physical deployment](dell-deployment.md) for the exact before/after values.
Wake on AC now enables startup when AC power is connected. It does not provide a
remote switch for the external power supply. BIOS image replacement is separate.

The installed Linux system loads `kernel.panic=15`, `kernel.panic_on_oops=1`,
`kernel.nmi_watchdog=1`, `kernel.softlockup_panic=1` and `kernel.hardlockup_panic=1`
at boot. These configure recovery after fatal kernel failures and detected CPU
lockups; an arbitrary hardware freeze can still prevent recovery.

Root SSH is available when the laptop is running and reachable. This does not
keep a Codex task continuously running or create an unattended cloud agent.

Source: [OpenRC daemon supervision](https://github.com/OpenRC/openrc/blob/master/supervise-daemon-guide.md).

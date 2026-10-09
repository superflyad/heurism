# Firmware access and connection hardening

Root SSH remained available on the installed Dell at 10.8.22.238. BIOS settings
are accessible through the supported Dell WMI interface, independently of
whether the motherboard firmware can be replaced. Two Python tools are installed
on the main system:

```powershell
python tools/dell.py --command 'companion-bios list'
python tools/dell.py --command 'companion-bios get PowerWarn'
python tools/dell.py --command 'companion-firmware-audit'
```

`companion-bios set <name> <value>` accepts only exact, exposed enum alternatives.
It records the previous value and requested change before writing, reports
readback and pending reboot, and skips writes when the setting already matches.
The change log is `/var/lib/companion/bios-changes.jsonl`. The existing Disabled
PowerWarn setting was verified through the tool without changing it.

Source tools are in `tools/target/`. On the Dell they reside in
`/etc/companion/tools/`, linked under `/usr/local/sbin/`. Python is installed on
the management system; these tools are not part of the RAM rescue package set.
Rescue can still read the same kernel sysfs firmware attributes directly.

## Confirmed firmware boundary

The audit records 90 exposed BIOS attributes, BIOS authentication-enabled flags,
MEI clients and firmware version, and reads MSR 0x13A through the Linux MSR driver.
It does not write CPU registers or probe/program flash chips.

All eight logical CPUs returned `0x000000030000007f`. Coreboot's CBnT decoder maps
bit 5 to measured boot, bit 6 to verified boot, and bit 32 to Boot Guard capability.
All three are set. This establishes active verified-boot policy on this boot;
the OEM key/fuse settings and exact protected flash regions have not been fully
audited. Root privilege and disabled UEFI Secure Boot do not establish permission
to run an arbitrary replacement BIOS. Our unsigned EFI application and OS loader
already work after Dell's hardware initialization.

The Intel SPI controller is present as 8086:a0a4. The installed kernel lacks
CONFIG_SPI_INTEL_PCI/PLATFORM, and no flash MTD interface was exposed. No BIOS
image was read or written. The local AMT port probe found no listener on 16992;
this alone does not prove hardware support or its absence. No independent AMT
or BIOS-screen channel has been established. KVM is available for isolated CPU
and kernel experiments while the host management system remains running.

Evidence: `artifacts/hardware/dell-firmware-access.json`, also stored on the Dell
at `/var/lib/companion/firmware-audit.json`.

Sources: [coreboot CBnT register definitions](https://github.com/coreboot/coreboot/blob/main/src/security/intel/cbnt/logging.c),
[coreboot firmware porting constraints](https://doc.coreboot.org/getting_started/faq.html).

## Access recovery beyond a stopped process

The watcher now performs a bounded localhost SSH key exchange and compares its
result with the pinned host public key. Three consecutive failures restart the
listener. A frozen listener is resumed before the normal service stop, avoiding
a stop operation blocked on the deliberately frozen process. SSH is also an
independent default-runlevel service.

On the physical Dell, SIGSTOP was delivered to listener PID 3677. A separate
100-second fallback was scheduled to resume it if the new guard failed. The
watcher recovered first: at 18:50:36 UTC it restarted SSH, and authenticated root
access returned at 18:50:38 with listener PID 5738 and the unchanged boot ID
`d69163a3-bdf3-4973-b6fd-aa830f344da7`. A new listener PID distinguishes watcher
recovery from the fallback merely resuming the old process.

Boot health now checks the actual host private key's public identity, the expected
client authorization, effective root/public-key SSH configuration, a working SSH
key exchange, the network watcher and an IPv4 lease. Public expected identities
reside under `/etc/companion/`; no client private key is copied to the Dell.

An independent boot-runlevel deadline waits 120 seconds. If this installed boot
has no matching healthy-boot ID and its EFI journal is still unconfirmed, it
syncs and forces a software reset into internal rescue. A confirmed boot cannot
later be reset by this one-shot guard, including after deliberate rescue selection.
The guard does not run in RAM rescue. It relies on the running Linux kernel and
does not recover a pre-userspace hang or a total hardware freeze.

A clean physical reboot with these services returned boot ID
`5f83413e-eb06-491d-824f-025853be075c`, installed ext4 root and a confirmed journal.

The actual main SSH configuration was then deliberately made invalid and the
Dell rebooted. At 18:58:01 UTC, the guard reset unconfirmed boot
`c4425aba-1af6-4630-ba80-11074e7afcc6` after its 120-second deadline. Root SSH
returned in internal RAM rescue with boot ID
`7906b021-45ce-4b57-8e2c-9be7f15afd77`. The main root was mounted read-only to
verify both the guard log and the invalid option that still prevented its SSH
server from starting. The saved configuration was restored remotely, then the
stable boot selected again. No USB or physical assistance was required.
Evidence: `artifacts/hardware/dell-bad-ssh-boot-rescue-proof.txt`.

The restored management system returned with boot ID
`d359b17b-f6b8-4ea0-9d7c-923b196f01fd`, valid SSH, the installed ext4 root,
independent SSH startup, the watcher, boot health and a confirmed EFI journal.
Final service and firmware facts are saved in
`artifacts/hardware/dell-access-progress-final.txt`.

The confirmed boot also remained online beyond the 120-second deadline. A healthy
deadline job completes and clears its daemon state; its status command reports
completion rather than a failed/crashed long-running service. Updated Python
tools passed compilation, and the full virtual USB/install/USB-removed regression
test passed root SSH, keyboard login, DHCP/SSH recovery and disk erase guards.

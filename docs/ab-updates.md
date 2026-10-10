# VM A/B boot prototype

The fresh Heurism VM image now has four GPT partitions: a 512 MiB EFI system
partition, 8 GiB root A, 8 GiB root B, and a 7.5 GiB ext4 data partition.
`/var/lib/companion` is on data, including the desktop user's home and saved
settings. Each root has its own kernel, initramfs, package database, C release,
`/etc/fstab`, slot identity and protected manifest. GRUB is installed from A;
its menu can load either root by UUID. A is the default. `grub-reboot 1` from A
sets a one-time B boot in A's GRUB environment block. The menu clears that
request before starting B, so the next reset returns to A. B's kernel has
`panic=15` for fatal startup failures. This is a prototype boot mechanism,
not yet an update installer or a promoted B release.

The candidate VHDX SHA256 is
`1e7f1235bbe9b0374cf5f60e126367642e1be6ed5a4602759a73ddd4117dda5b`.
It was built on PrimeLinux from `tools/build-hyperv-guest.sh` and attached to
the isolated Hyper-V `HeurismCandidate` VM, ID
`ef556cc9-85df-484d-b765-cc3070a315a2`. The normal `CompanionDev` VM had
checkpoint `Heurism-before-AB-test-20261010`, UUID
`7677db03-ca2a-43cd-ae4e-9e0cca52a109`, before it was stopped for the
candidate's use of the same private IP.

Live test sequence on 2026-10-09 local / 2026-10-10 UTC:

1. Default A boot `9245708d-067e-4817-a2fb-92990ecff0d3` mounted
   `/dev/sda2` as root and `/dev/sda4` as data. SSH, watch, control, desktop,
   C release and protected hashes passed. A data marker was written.
2. `grub-reboot 1` and a VM restart booted B on `/dev/sda3` at
   `aebd6717-2cc2-48a7-8c79-35d53132f043`. B read the same data marker,
   passed protected hashes and C release health.
3. A further VM restart returned healthy A on
   `044f4aca-552d-4d2a-9251-caab781e8c40`, with the marker intact and A's
   one-time GRUB selection cleared.
4. On A, only B's `/sbin/init` was replaced with a script that exits. A second
   one-time B request was made. After the fatal B startup, the VM returned to
   healthy A on `ac1a32a7-6ae3-4b51-8108-fcef230c75d6`; SSH, watch,
   control, desktop, protected hashes and the data marker survived. B's
   original init symlink was restored from its saved copy. No second host
   reset was issued. The intervening panic screen was not captured, so the
   observed result is the returned healthy A boot, not a timed panic trace.
   This does not prove recovery from every possible hang or storage/firmware
   fault.

`HeurismCandidate` was then shut down. `CompanionDev` resumed with its
original pinned SSH identity, release and desktop on fresh boot
`c8cfc619-4ca4-4889-b285-f8590445490c`. No Dell partitions, boot entries,
kernel or NVRAM were changed.

## Remaining gates

The bootloader and mutable GRUB environment are anchored on root A. If A or
the EFI partition fails, B may be unreachable. A production update path needs
offline staging into the inactive root, validation of package/boot files,
health-based promotion, a watchdog for hangs, and a recovery route that does
not depend on guest SSH or the same disk. Shared data also needs a versioned
migration/rollback policy. The current Dell has a single ext4 root and a
history of a physical firmware-logo halt; this VM result does not authorize
unattended Dell repartitioning or boot-path replacement.

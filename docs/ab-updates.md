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

## C slot control and bounded VM recovery

The next fresh image adds `heurism-slot`, a C executable installed only in the
VM image. It accepts `status`, `verify-inactive`, `queue`, `renew` and
`fallback`. Before touching the boot environment it checks root identity,
the exact Hyper-V development profile, all four expected mounted volumes,
the inactive root's slot marker, protected manifest and sealed C release,
plus the required default-runlevel service links. `queue` runs only on A and
sets B for one boot. `renew` runs only on B after at least 45 seconds of
uptime with matching current watch boot ID, protected hashes, C release,
desktop, control socket and SSH/watch service health. It grants B one more
boot by writing A's GRUB environment. `fallback` clears that request. No
automatic service invokes `renew` yet.

The host-only `tools/ab-vm-watch.py` is an explicit transaction gate for the
isolated candidate VM. It requires the exact VM identities and attached disk,
the candidate's pinned SSH key and the previous boot ID. If no healthy new
boot appears before the deadline, Hyper-V resets the candidate and checks
that A returns. It is not a continuously running watcher or a Dell reset
channel.

The intermediate C-slot image SHA256 was
`bec62743f921e2c5e7c81ca8e9d13e948c4161e6d0bd60b1ffa0e527d290641b`.
It booted A `3ba8694f-e6fc-4044-80da-20e2d3d20426`, rejected a corrupt B
slot marker, booted healthy B `f03cc5ea-d5c8-4629-874b-4d80bffac6c7`,
renewed B after 55 seconds, and booted healthy B again on
`3e8b6638-5c10-487b-b7e2-cef61bf19c30`. The next boot returned healthy A
`d2fe509a-bbf3-46fa-9a84-01e43b7faf7a` without renewal. A deliberate B
init that slept indefinitely prevented guest SSH; the 50-second host gate
reset the VM and returned healthy A `a8bc0c3b-2cad-4ac0-a450-c0c905209643`
with the shared data marker intact. B's original init symlink was restored.

That test revealed that the first manifest did not cover `/sbin/init`. The
final image adds it and checks the default service links before queueing B.
Final candidate VHDX SHA256 is
`bde276fe736d31f6142f7f0b500f25fb3b41a6824bd843063049f8d19aadc023`.
Its A boot `6e29bff7-4449-4889-a73c-90d6fcfb3abb` rejected an altered B
init and a missing B SSH service link, then verified B after both fixtures
were restored. It booted healthy B
`7ea3012c-4ff8-4e41-ab60-4e46a1ad9310`, read the shared data marker,
renewed after 50 seconds, then used `fallback` to select A. A fresh healthy
boot `dc54e9b7-fd5a-450b-a098-6b60cda64d1e` also passed the host watcher's
new previous-boot-ID guard. `HeurismCandidate` is off. `CompanionDev` resumed
its original pinned identity and C/Xfce release on boot
`ab3f3709-0a9b-486f-978b-c289430add29`. Its pretest checkpoint is
`Heurism-before-C-slot-test-20261010`, UUID
`89996342-4a83-4db3-a158-ce448d3e1986`.

This gate verifies and selects a slot; it does not yet stage signed package
or C runtime updates into B. Renewal is manual, and the host watcher must be
invoked for each candidate boot. The Dell and normal VM retain their existing
single-root disks. A still anchors GRUB, and no physical independent reset
exists.

# Heurism NVRAM name migration

## Prepared candidate, 2026-10-09

The candidate SSD bootstrap in `boot/nv_startup.c` tries
`HeurismExtensionImage01` first, then the installed
`CompanionExtensionImage01`, under the existing owner GUID
`1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1`. Both reads require attributes 7,
exactly 2,048 bytes and an exact match to the pinned owner image. It executes
only bytes retrieved from a verified variable. A missing or rejected new
variable leaves the legacy variable available; if neither is valid, the
existing embedded SSD service remains the fallback. The 64-byte volatile
`CompanionNvStartup01` marker keeps version 1 and distinguishes legacy NVRAM
(path 1), SSD fallback (path 2) and Heurism NVRAM (path 3).

The image and owner protocol still contain their original Companion ABI. This
candidate migrates the variable lookup name only. Its bootstrap still depends
on the SSD. A future payload/ABI rename needs its own compatibility and
recovery tests; this build does not claim that work is finished.

The candidate EFI SHA256 is
`a215f4143742e4263577942e1886b5546375bf08777f11903196e0f99c78ff31`.
Nineteen host cases passed. Six isolated OVMF boots passed: valid legacy,
missing both, corrupt legacy, valid Heurism, corrupt Heurism with valid legacy,
and corrupt both. These are actual EFI `LoadImage`/`StartImage` tests; they do
not establish Dell firmware behavior. Build and test reports are under ignored
`build/nv-startup/`.

The Dell's current boot ID is
`d5e85973-2087-4fc9-ab40-d43251868a3a`. A read-only variable snapshot is
saved under ignored
`artifacts/firmware/boot-variable-backups/20261009T133605Z/`. It verified
the persistent legacy payload, BootOrder `0005,0000`, DriverOrder
`0000,0001`, absent BootNext, healthy SSH/watch and recovery loader hashes.
The current active SSD driver still hashes to
`2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4`;
the six-file protected manifest passes. The new variable does not exist on
the Dell. A copy of the candidate binary is staged at
`/var/lib/companion/firmware/heurism-nv-candidate/dual-name.efi` in a mode
0700 root-owned directory, with the file mode 0600. It is not on the EFI
partition and is not selected for boot. The guarded variable-stage script
`tools/target/heurism-nv-stage.sh` is staged beside it as `stage-variable.sh`.
Its shell syntax and read-only `check` action passed on the live Dell. Its
`create` action has not been run.

## Physical activation gate

No new NVRAM variable has been written and no EFI boot driver has been
replaced. Before either operation, arrange physical access to the Dell power
button and local recovery screen. SSH and the Linux watcher cannot recover a
firmware hang before Linux starts; Wake-on-LAN from full poweroff is not
established. The owner previously asked to keep the machine running when a
power-button recovery would not be available.

The guarded rollout sequence is: recheck board identity, current boot health,
EFI file hashes, exact boot/driver order and variable-store capacity; retain
the legacy variable and active SSD driver; create the Heurism variable with
the same pinned payload in a single efivarfs write, then verify its attributes
and readback; boot once
with the old driver; then install the tested dual-name driver with a backup
and updated six-file protected manifest. On the next normal boot, require
marker path 3, fresh boot ID, SSH/watch/control/Xfce health, unchanged loader
and kernel hashes, and the verified SSD boot order. Retain the legacy variable
until multiple good physical boots and a recovery test. Do not select a NIC
BootNext entry or remove the legacy variable as part of initial activation.
Linux's [efivarfs documentation](https://docs.kernel.org/filesystems/efivarfs.html)
explains the four-byte attribute prefix and warns that deleting nonstandard
variables can expose firmware bugs. The kernel's
[EFI variable example](https://docs.kernel.org/5.4/admin-guide/acpi/ssdt-overlays.html)
also specifies a single write for the complete variable. The rollout must
follow those interfaces and must not use a multi-write copy to create the new
variable.

The stage script copies the verified 2,052-byte legacy efivarfs file to a
root-only regular file, then uses one `dd` block of that exact size to create
the new name. It verifies both variables byte for byte afterward and never
deletes or overwrites either variable. A creation error stops the rollout for
inspection; it must not be followed automatically by EFI driver activation.

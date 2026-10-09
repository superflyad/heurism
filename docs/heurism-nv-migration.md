# Heurism NVRAM name migration

## Active Dell startup, 2026-10-09

The active SSD bootstrap in `boot/nv_startup.c` tries
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
release migrates the variable lookup name only. Its bootstrap still depends
on the SSD. A future payload/ABI rename needs its own compatibility and
recovery tests; this build does not claim that work is finished.

The active EFI SHA256 is
`a215f4143742e4263577942e1886b5546375bf08777f11903196e0f99c78ff31`.
Nineteen host cases passed. Six isolated OVMF boots passed: valid legacy,
missing both, corrupt legacy, valid Heurism, corrupt Heurism with valid legacy,
and corrupt both. These are actual EFI `LoadImage`/`StartImage` tests; they do
not establish Dell firmware behavior. Build and test reports are under ignored
`build/nv-startup/`.

The pre-change boot was `d5e85973-2087-4fc9-ab40-d43251868a3a` and its
read-only variable backup is under ignored
`artifacts/firmware/boot-variable-backups/20261009T134215Z/`. The new
variable was then created with the same 2,048 payload bytes and attributes 7.
It matched the legacy variable byte for byte. A normal checked reboot under
the **old** driver returned boot `acb8eaf4-8627-4751-b12c-ef838ec56122`
with both variables persisted, SSH/watch/control/Xfce healthy and the old
protected manifest passing. Dell appended its exact known USB NIC entries;
BootOrder was restored to `0005,0000` after checking their MAC and paths.

The guarded `tools/target/heurism-nv-activate.sh` then backed up the old SSD
driver and protected manifest, installed the tested dual-name driver and
updated the driver line in the six-file manifest. Its live verification and C
power check passed before reboot. The old driver SHA256
`2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4`
is backed up at
`/boot/efi/EFI/companion/companionextx64.before-heurism.efi` and under the
root-only candidate directory. The manifest backup is also in that directory.

The second normal checked reboot returned fresh boot
`1ae2b1d9-a8ce-4467-97be-e952e7198888`. The volatile marker reports
**path 3** and EFI_SUCCESS for variable read, LoadImage, StartImage and service
verification. Thus this physical boot executed the verified Heurism variable.
The active driver and six-file protected manifest pass; SSH/watch/control,
Heurism Xfce session and speaker sink are healthy. BootCurrent is `0005`,
BootOrder was restored to `0005,0000` after verifying the same appended USB
NIC entries, DriverOrder is `0000,0001`, and BootNext is absent. Both payload
variables remain identical. The post-boot variable snapshot is under ignored
`artifacts/firmware/boot-variable-backups/20261009T134701Z/`; parsed marker
and management evidence is in ignored
`artifacts/firmware/heurism-nv-verification.json`.

The tested binary remains staged at
`/var/lib/companion/firmware/heurism-nv-candidate/dual-name.efi` in a mode
0700 root-owned directory, with the file mode 0600. The stage and activation
scripts are staged beside it. The EFI partition contains the active copy and
the old-driver backup.

## Recovery boundary

The owner confirmed physical power-button access for this activation. SSH and
the Linux watcher still cannot recover a firmware hang before Linux starts;
Wake-on-LAN from full poweroff remains unestablished. The installed driver
depends on the SSD bootstrap. No NIC boot option was selected, no BootNext was
set, and the legacy payload variable was not deleted. The original owner
image and protocol ABI and the volatile marker retain their Companion names.

The legacy variable and old SSD driver backup remain available for rollback.
The `rollback` action of `tools/target/heurism-nv-activate.sh` can restore the
old driver and protected manifest when root SSH is available; it does not
remove the new variable. That rollback action was not exercised on the Dell.
Do not delete the old variable until a separate, proven recovery migration.
Linux's [efivarfs documentation](https://docs.kernel.org/filesystems/efivarfs.html)
explains the four-byte attribute prefix and warns that deleting nonstandard
variables can expose firmware bugs. The kernel's
[EFI variable example](https://docs.kernel.org/5.4/admin-guide/acpi/ssdt-overlays.html)
also specifies a single write for the complete variable. The rollout used
those interfaces.

The stage script copied the verified 2,052-byte legacy efivarfs file to a
root-only regular file, then used one `dd` block of that exact size to create
the new name. It verified both variables byte for byte afterward and did not
delete or overwrite either variable. Its failure path stops before EFI driver
activation.

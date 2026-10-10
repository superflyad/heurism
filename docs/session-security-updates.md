# Session security and whole-system updates — 2026-10-09

## Dell session

The local root account previously had an empty password. The pinned SSH key
path was verified before a random local root password was installed. The
desktop account, `companion-ui`, also received a distinct random password for
PAM screen unlock. The owner-only recovery files are:

- `artifacts/ssh/dell-root-console-recovery.txt`
- `artifacts/ssh/dell-desktop-unlock.txt`

Both are ignored by Git and have Windows ACLs restricted to CORP\\michael,
Administrators and SYSTEM. Keep copies in an owner-controlled password store
before removing this workspace. The account passwords were not printed in
the install log. Root SSH remains key-only and was checked after both changes.

The root-owned `/etc/heurism/desktop-lock` marker enables the locked session.
The Heurism user session waits for `xfce4-screensaver` to report an active
lock before recording UI health. If Xfce or the locker fails during startup,
it does not fall through to the unlocked native or legacy recovery UI. The
running session watches the locker process and ends the graphical session if
it exits; OpenRC starts a new, locked session. Root SSH and watch are separate.
The marker, package and saved Xfce settings configure a five-minute idle
timeout with immediate locking.

VM testing showed the actual password dialog, successful PAM unlock, locked
startup after a hard reset and relocking after a forced locker crash. On the
Dell, installation, active lock query and forced-crash relocking passed
without a machine reboot. A physical keyboard unlock and Dell boot persistence
remain to be observed. The X11 shared-UID and unencrypted-disk limits are
described in [security.md](security.md).

## VM system upgrade

`python tools/prime-vm.py upgrade-system` is host-only VM orchestration. It
checks the exact Hyper-V identity, current C release and health, creates a
Hyper-V checkpoint, accepts only the configured Alpine v3.24 main/community
repositories, and runs signed `apk` package updates. It preserves and checks
Heurism's custom GRUB menu because the upstream GRUB trigger rewrites it on a
kernel update. It refreshes the protected package/kernel hashes, checks root
management and UI services, reboots, waits for a fresh boot, and captures a
painted frame. If any stage fails, the host restores the disk/memory
checkpoint, forces a fresh boot and rechecks independent management and UI.

The first rehearsal rejected the GRUB trigger's changed menu before boot and
the previous kernel returned from a host checkpoint. After preserving the
custom menu, a deliberate post-upgrade failure rolled back the installed
packages and returned a fresh healthy boot. A later rollback exercise returned
fresh boot `4e7e9c6b-227d-48ca-9572-38172bd1a6a7` with kernel `6.18.55-r0`.
The successful transaction upgraded 15 packages, including Linux LTS
`6.18.53-r0` to `6.18.55-r0`, and booted
`673f4c8b-95e1-4543-a848-883599726b8f` with SSH/watch/control/Xfce,
protected hashes and painted framebuffer. Its retained rollback checkpoint
is `Heurism-system-upgrade-20261010T012401Z`, UUID
`c3863b8a-3c53-4d12-a80a-8b216ee9a928`.

The `upgrade-system` command targets only `CompanionDev`; it does not update
the Dell. A host Hyper-V checkpoint is a complete VM rollback, but it is not
an on-device A/B system. Dell root is one ext4 partition and its SSD boot
path has had a physical firmware halt. Before a Dell kernel/OS upgrade,
build and test an alternate root plus persistent data layout, one-shot boot
selection, automatic failure return, and a recovery channel that works after
lost guest SSH. Do not substitute the C release symlink rollback for this.

## Fresh image

The image builder now requires unique 128-bit random passwords supplied by
the Windows host for local root recovery and desktop unlock. It locks the
Xfce session on first boot and includes the PAM-backed locker. The host
password files have restricted ACLs. Their staged plaintext copies on
PrimeLinux are removed after account provisioning. The first failed staging
attempt and the successful tested build had their staged password files
removed manually; the builder now performs that removal itself.

Built VHDX SHA256:
`50aa6535210a33adcf6c01a2b5f310b3020a3b3f4042d104efae6450380cd4e5`.
It booted in isolated `HeurismCandidate` VM `ef556cc9-85df-484d-b765-cc3070a315a2`
to boot `b23c66b5-f4db-4e92-bad4-6350a4275984`. The two password hashes,
lock marker and package, C release, protected files, SSH/watch/control and
Xfce health passed; the locker reported active. The candidate was powered
off and `CompanionDev` returned with its original pinned SSH identity on
fresh boot `bd7ea378-cc13-416c-aab6-aafb7dbd9739`.

That image was a local candidate with a single ext4 root. A later fresh image
has a separate A/B root and persistent data partition, with a live one-time
boot and fatal-startup fallback test described in [A/B updates](ab-updates.md).
It still embeds a host-specific SSH authorized key and lacks a pinned package
snapshot. It is not a distributable installer or a Dell migration image.

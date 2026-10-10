# Heurism system base

Heurism is the operating system we are building for the Dell. Linux remains its
kernel. Alpine 3.24.2 is currently the upstream source for the kernel, package
manager, base Unix tools and many applications. Xfce is the established desktop
session until Heurism has an equally usable alternative. The active Heurism
shell, terminal, controls, settings and release verifier are C programs.

The fresh VM image now has unique local account passwords, a locked Xfce
session and a VM-tested A/B root plus persistent data layout. `CompanionDev`
has a separate host-checkpoint system upgrade/rollback path. See the
[A/B boot test](ab-updates.md) for its exact scope.
The Dell still has a single ext4 root and no independent full-OS rollback.
Current evidence and the physical migration gate are in
[session security and updates](session-security-updates.md). The older image
candidate and future gates below describe the earlier baseline.

## Identity and provenance

On 2026-10-09 the VM and Dell installed `platform/heurism/os-release` as the
regular `/etc/os-release`. It reports `ID=heurism` and `ID_LIKE=alpine`.
The packaged `/usr/lib/os-release` still records Alpine 3.24.2; Heurism's
`/etc/heurism/upstream-release` records the base and `apk` package manager.
Neither file claims a Heurism version number before a system release scheme
exists. Both Heurism files are covered by the respective protected manifests.
The guarded installer is `tools/target/heurism-system-identity.sh`.

This identifies the installed system accurately. It does not make Alpine
packages, the Xfce session or the image build process Heurism-native by itself.

## Build ownership

`platform/heurism/vm-packages.list` selects 47 direct packages for the next
Hyper-V VM image. `tools/prime-vm.py` stages the package profile, C runtime
source, system identity and wallpaper. `tools/build-hyperv-guest.sh` verifies
staged hashes and package names before using `apk`. It starts from the pinned
Alpine 3.24.2 minirootfs and Alpine 3.24 repositories, installs
`linux-firmware-none`, compiles the C runtime with temporary build packages,
and seals that release before first boot with
`userspace/native/install-image-vm.sh`. The image starts SSH, the independent
watch service, C control and the Xfce user session. An unhealthy Xfce session
falls back to the C workspace. It does not install the old Python/Tk desktop.
Onboard is an upstream application that pulls in Python 3; no Heurism runtime
component uses Python. The image records all installed package versions and
the base/source/profile hashes under `/etc/heurism`; these records and the
wallpaper are in its protected manifest. The installed inventory is a record,
not a repository lock. The Alpine v3.24 URLs do not pin every package build.
The revised profile adds `font-wqy-zenhei` for C terminal glyph fallback; the
existing image candidate predates that profile and has not been rebuilt with it.

The first C-first candidate image SHA256 is
`5bce37d38af77435b65e99f999284a9560c70216fa08e38ebfd4bfc9882f9e8c`.
It is an isolated test artifact, not a public image: its root authorized key
and generated SSH host private key are baked into the VHDX. A distributable
image needs per-install key provisioning and a repeatable package source.

## Verified live state

The fresh image booted in the separate `HeurismCandidate` Hyper-V VM from an
exact hash-checked disk. First boot `6edd8116-4960-4bc0-9d64-392e6f304c19`
passed C release verification, Xfce UID-1000 health, actual 1280x800
framebuffer inspection with the Heurism wallpaper, SSH/watch/control services,
checked power, protected-file hashes and exact installed-package inventory.
The old Tk desktop and `python3-tkinter` are absent. An invalid session
selection started the painted C workspace with Heurism branding while root
management stayed healthy. A UID-1000 C shell created a persistent file, and
the C graphical terminal opened an X11 window. Restoring the session mode
returned Xfce. Checked C reboot returned fresh boot
`9ba17212-8231-4d66-8863-1a5cfb3c4935` with the same sealed release,
healthy services, power/protected checks and the user file intact. Checked C
poweroff reached the Hyper-V Off state; the original `CompanionDev` VM was
then restarted on fresh boot `9b727da7-1024-40dd-97b8-5fc3186c7a42` with
its existing C release and Xfce health. Its pretest checkpoint is
`Heurism-before-image-candidate-20261009` (UUID
`a90e8c9e-73d7-4967-bb17-fb3eb5e66e1b`). The candidate was never installed
on the Dell.

The latest local image candidate SHA256 is
`1c0cef7e9a02d369d19b306a7ee25ddf1575c9b6678b7504489c50f94809569c`.
It adds 512-line bounded C terminal scrollback and corrects host staging of
Unix text when Windows checks out CRLF files. Earlier isolated terminal
screenshots showed lines 60–80 live, 39–60 after Shift+PageUp and 36–57 after
one wheel step. Shift+PageDown restored the prompt and a new command wrote a
UID-1000 file. Seven hundred output lines and a resize left the terminal
healthy with 8.5 MiB resident memory in that observation. A narrow-to-wide
resize exposed a black repaint patch in a later candidate; blank backfill
cells now have unit width and default colors. The corrected binary passed the
same live resize check, then the exact sealed image booted on
`5fbcdc7a-b640-4eda-b4db-d8d00ce9626e` with release
`/opt/heurism/native/releases/heurism-os-image-20261009T152521Z`. Its actual
framebuffer had the normal terminal background after narrow-to-wide resize;
release, Xfce, package inventory, protected files and C power checks passed.
Checked poweroff reached host-observed Off. `CompanionDev` returned on fresh
boot `0b1b0863-01f4-4406-a6bd-a9a2c37e4cc7` with its existing release and
Xfce healthy. This local candidate was not installed on the Dell or published.

The VM passed the identity installer and a checked fresh boot
`82e62200-48b6-44f5-9562-78f8c55ad056`. Its Heurism C release, Xfce health,
SSH/watch/control, protected manifest, `apk` and power checks passed. A
prechange checkpoint exists as `Heurism-before-system-identity-20261009`, UUID
`dde44829-5871-4f1b-8118-e69ddff74e8a`.

The Dell passed the installer and a checked fresh boot
`4dc58594-e232-44f6-baa2-6998c17115cb`. Its C release, Xfce health,
SSH/watch/control, speaker sink, protected manifest, `apk` and power checks
passed. The EFI NVRAM startup marker still reports successful Heurism variable
read path 3. After the firmware appended verified NIC boot entries, BootOrder
was restored to `0005,0000`; DriverOrder is `0000,0001` and BootNext is absent.
The EFI bootstrap still depends on the SSD. Legacy service, path, account and
firmware protocol names remain recovery interfaces.

## Next system release gates

1. Provide first-boot SSH key provisioning and a package lock or repository
   snapshot so a public image can be reproduced and safely redistributed.
2. Prove a whole-system update and rollback path, including failed package
   updates, without losing independent management or the previous C release.
3. Define a Heurism system release number and signed update inputs.
4. Promote the proven system release to the Dell through its guarded installer,
   retaining SSD rescue, authenticated root management and the current boot
   orders. Do not use unverified native kernel or USB drivers as the Dell default.

See [architecture](architecture.md), [VM operations](prime-vm.md),
[native C runtime](native-runtime.md) and [security](security.md).

# Heurism system base

Heurism is the operating system we are building for the Dell. Linux remains its
kernel. Alpine 3.24.2 is currently the upstream source for the kernel, package
manager, base Unix tools and many applications. Xfce is the established desktop
session until Heurism has an equally usable alternative. The active Heurism
shell, terminal, controls, settings and release verifier are C programs.

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

`platform/heurism/vm-packages.list` is the direct package selection for a new
Hyper-V VM image. `tools/prime-vm.py` stages it and `tools/build-hyperv-guest.sh`
checks its hash and package names before calling `apk`. The image builder still
starts from an Alpine 3.24.2 minirootfs and Alpine 3.24 repositories. It first
installs `linux-firmware-none`, then the profile's 37 direct packages. The
installed image gets Heurism identity and upstream provenance at image build
time. This builder change has had syntax and source checks; a fresh candidate
image has not yet been built or booted from this revision.

The current image builder also stages the historical Python/Tk desktop as a
base and relies on a later C release installation. The live VM and Dell use
the C desktop/control release; Python/Tk remains sealed recovery. Removing
that build-time gap requires a Heurism image recipe that installs a sealed C
release before first boot, with management and rollback verified in an
isolated VM. A package lock or snapshot repository is also needed for repeatable
updates; the current v3.24 repository URLs alone do not pin every package build.

## Verified live state

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

1. Build a fresh Heurism VM candidate from the checked package profile and a
   sealed C release, with no active Python/Tk startup path.
2. Boot that candidate in isolation and prove SSH/watch/control, desktop first
   paint, power, file workflows and protected-file checks across cold start and
   reboot. Preserve the running VM's pinned SSH host identity.
3. Define a Heurism system release number, package lock or repository snapshot,
   signed update inputs, and atomic rollback for base packages and C runtime.
4. Promote the proven system release to the Dell through its guarded installer,
   retaining SSD rescue, authenticated root management and the current boot
   orders. Do not use unverified native kernel or USB drivers as the Dell default.

See [architecture](architecture.md), [VM operations](prime-vm.md),
[native C runtime](native-runtime.md) and [security](security.md).

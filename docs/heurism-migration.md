# Heurism naming migration

Heurism is the product name. Alpine Linux remains the kernel and Unix base;
Xfce 4.20 is the default desktop. Heurism owned runtime programs are C.

## Active releases

| Target | Sealed release | Fresh boot verified |
| --- | --- | --- |
| Hyper-V `CompanionDev` | `/opt/heurism/native/releases/heurism-os-20261009T015910Z-3115` | `44ae8e38-07e5-4acc-bacd-4efa95d0a528` |
| Dell Inspiron 7506 2n1 | `/opt/heurism/native/releases/heurism-os-dell-20261009T020254Z-7718` | `d5e85973-2087-4fc9-ab40-d43251868a3a` |

The active symlink is `/opt/heurism/native/current` on both targets. The C
executables are `heurism-sh`, `heurism-terminal`, `heurism-control`,
`heurismctl`, `heurism-desktop`, `heurism-app`, `heurism-session-config` and
`heurism-release`. Files and Editor are hardlinks to `heurism-app` in each
sealed release. The Xfce application entries display Heurism Settings,
Heurism Terminal and Heurism Power. `/usr/local/bin` links expose the shell,
terminal, control client and release verifier. The new control socket is
`/run/heurism-desktop/control.sock`.

The VM candidate passed strict C compilation, shell and interactive shell
checks, isolated Xfce windows, C control socket checks, sealed verification,
activation and a checked reboot. The Dell candidate passed the corresponding
isolated Dell control, Xfce, Files/Editor and sealed corruption checks. The
first Dell activation failed its UI gate and automatically restored the old
release. A subsequent candidate activated but exposed an X authority startup
bug after reboot; root SSH/watch stayed healthy and the previous service
scripts and desktop were restored from the guarded install backup. The final
candidate chooses the active X authority for each C control command, retries
saved input settings briefly and marks failed session startup for recovery.
The panel update installs a separate Heurism Power launcher and retains the
old launcher file for recovery. Its original release passed checked reboot,
release and UI health,
input settings, speaker sink, checked power, SSH/watch/control services, and
all six protected EFI/kernel hashes. Firmware appended its known auto-created
USB NIC entries 0001 and 0002 to BootOrder; after their exact MAC and paths
were checked, BootOrder was restored to `0005,0000`. BootCurrent is `0005`,
DriverOrder is `0000,0001`, and BootNext is absent. Temporary build packages
were removed from both machines.
The live Dell Heurism Power window opened and closed with Escape without
requesting a power action. The active control socket is mode `0660` under a
mode `0750` root-owned directory, with `companion-ui` as the allowed group.

## Management identity and service migration, 2026-10-08

The installed hostnames are `heurism-vm` and `heurism-dell`. The Dell now sends
`heurism-dell` in its DHCP requests; this was checked in the live Ethernet DHCP
process after the final fresh boot. The pinned SSH host key and local
discovery alias are unchanged. The root management status, local welcome screen,
audit heading and next-boot usage show Heurism. `heurism-status`,
`heurism-audit`, `heurism-wifi` and `heurism-next-boot` are installed command
links; the old command paths remain for boot and rescue compatibility.

Both machines now start the C runtime through `heurism-control` and
`heurism-desktop` OpenRC services. The old desktop/control service files remain
installed but are disabled. An isolated VM migration passed a checked reboot,
then a subsequent release activation reused the new service names. The Dell
migration passed the same gate, a checked reboot, a second candidate's sidecar
tests and activation against the new service names, then a fresh checked boot
to the ID above. On that boot SSH, the independent watch, C control, Xfce UI,
release hashes, power gate and six protected EFI/kernel hashes passed.
Firmware appended the verified USB NIC entries to BootOrder; it was restored
to `0005,0000`. BootCurrent is `0005`, DriverOrder `0000,0001`, BootNext absent.

The root install staging directory was found mode `0777` with writable files.
It is now root-owned mode `0700` on both targets. Both installers reject a
writable stage or writable staged files; negative checks passed in the VM.
Temporary build headers were removed after the C guard update. The live Dell
control socket remains mode `0660` under a `0750` root-owned directory.

The Dell Boot0005 display label was tested with `efibootmgr -b 0005 -L`, which
returned success but left the label unchanged. The guarded check failed and
restored the original label. The saved raw Boot0005 variable and live variable
have the same SHA256 `1829612ab6ab9e639e8ab3fcb416942acaa60f0bca1639610acc76deb47815cb`.
The C power guard accepts either exact old or new display label, but the Dell
still displays `Companion Management`. No EFI loader path or protected file was
changed.

## NVRAM boundary, checked 2026-10-08

The Dell still stores the 2,048-byte owner EFI image in the nonvolatile variable
`CompanionExtensionImage01` under GUID
`1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1`. Its live attributes are 7 and its
payload SHA256 is still
`b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a`.
The separate `CompanionNvStartup01` variable has attributes 6; it is a volatile
startup diagnostic, not another persistent copy of the image.

The SSD `Driver0001` bootstrap reads the exact `CompanionExtensionImage01`
name in `boot/nv_startup.c`, verifies the stored image and executes the retrieved
bytes. It has an embedded SSD fallback. Renaming or deleting the persistent
variable alone would stop NVRAM image execution even if the normal Linux boot
still succeeded through that fallback. The current bootstrap also resides on
the SSD, so this arrangement does not provide SSD-independent startup.

No owner variable, EFI driver, DriverOrder or payload bytes were changed during
the Heurism runtime migration. A future firmware-layer rename needs a tested
bootstrap that can read both names, a matching new variable and verified image,
isolated VM fallback tests, protected-file manifest updates, and a guarded Dell
boot with the old path available for recovery before any old variable is
removed. Product UI and management now say Heurism; this firmware compatibility
identifier still says Companion.

The [dual-name NVRAM bootstrap](heurism-nv-migration.md) now runs on the Dell.
Physical boot `1ae2b1d9-a8ce-4467-97be-e952e7198888` verified execution
from `HeurismExtensionImage01`; the legacy variable and old SSD driver backup
remain for recovery.

## Installed interfaces retained during migration

The protected EFI loader, SSD kernel and recovery paths, Boot0005 display
label, `companion-ui` UID-1000 account, independent `companion-watch` and
healthy-boot journal, `/etc/companion` configuration, existing desktop
preferences and pinned SSH host identities retain their installed names. The
sealed Python/Tk recovery release also remains at its old path. Those paths
are inputs to boot recovery, verified manifests or UID-1000 fallback, and are
not user-facing product names. Replacing them requires a complete recovery
migration; do not rename EFI files or the sole Ethernet/SSH identity for a
cosmetic result.

Historical docs and recorded artifact paths preserve the names and hashes
under which their tests ran. The repository currently has a local Git history
on `task/heurism-rename`; no remote is configured, so it is not yet a backed-up
or shared repository. The working folder is still named `companion` while this
task uses that checkout.

The shared X11 UID-1000 session and unencrypted Dell root disk remain security
limits. This naming change does not create application isolation or disk
encryption.

# Heurism naming migration

Heurism is the product name. Alpine Linux remains the kernel and Unix base;
Xfce 4.20 is the default desktop. Heurism owned runtime programs are C.

## Active releases

| Target | Sealed release | Fresh boot verified |
| --- | --- | --- |
| Hyper-V `CompanionDev` | `/opt/heurism/native/releases/heurism-os-20261009T011559Z-4876` | `3791a271-7f76-415c-a832-b84450371a79` |
| Dell Inspiron 7506 2n1 | `/opt/heurism/native/releases/heurism-os-dell-20261009T011703Z-5716` | `e7052d51-3114-4c0f-8e06-a9a522d53bd1` |

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
old launcher file for recovery. It passed checked reboot to the fresh boot ID
above, release and UI health,
input settings, speaker sink, checked power, SSH/watch/control services, and
all six protected EFI/kernel hashes. Firmware appended its known auto-created
USB NIC entries 0001 and 0002 to BootOrder; after their exact MAC and paths
were checked, BootOrder was restored to `0005,0000`. BootCurrent is `0005`,
DriverOrder is `0000,0001`, and BootNext is absent. Temporary build packages
were removed from both machines.

## Installed interfaces retained during migration

The protected EFI loader, SSD kernel and recovery files, Boot0005 firmware
label, OpenRC service names, `companion-ui` UID-1000 account, root watch and
healthy-boot records, `/etc/companion` configuration, existing desktop
preferences, pinned SSH host identities and management hostnames still use
their installed names. The exact old Boot0005 label remains in the C power
check so the guard validates the real firmware entry. The sealed Python/Tk
recovery release also remains at its old path. These are compatibility and
recovery boundaries, not the Heurism product name. Changing them requires a
separate migration with cold-boot and rollback evidence; do not rename EFI
files or the sole Ethernet/SSH identity merely to change a label.

Historical docs and recorded artifact paths preserve the names and hashes
under which their tests ran. The repository currently has a local Git history
on `task/heurism-rename`; no remote is configured, so it is not yet a backed-up
or shared repository. The working folder is still named `companion` while this
task uses that checkout.

The shared X11 UID-1000 session and unencrypted Dell root disk remain security
limits. This naming change does not create application isolation or disk
encryption.

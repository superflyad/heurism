# Heurism product roadmap

Heurism currently builds from Alpine packages and uses Xfce as the established
desktop. Its shell, terminal, settings, control and release verification are C. The
Dell is the primary physical target; the separate Hyper-V VM is the release
proving ground. See [architecture](architecture.md),
[current release](heurism-migration.md), [security](security.md) and the
[working OS checklist](os-checklist.md).

| Area | Current state | Next useful acceptance |
| --- | --- | --- |
| System base | Heurism identity on VM and Dell; fresh C-first Hyper-V candidate booted and passed first paint, fallback, reboot and shutdown gates; Alpine 3.24.2 package source retained | Provision per-install SSH keys, pin a package source, and prove whole-system update and rollback without losing management |
| Daily desktop | Xfce behavior with a sealed Heurism wallpaper, mark, panel and dock visual pass proven in the VM and persistent across a checked Dell reboot | Give windows, launchers, notifications and settings one Heurism style; complete ordinary Dell tasks with human input |
| Shell | C command runner with editing, history, pipes, redirection, globbing, job control and bounded Tab completion; VM reboot and real Dell PTY checks passed | Expand quoted completion and POSIX behavior; keep BusyBox ash for system scripts |
| Terminal | C X11/Xft/libvterm PTY with bounded scrollback, title cue, selection and clipboard; real Dell PTY and cross-app paste passed without an OS reboot | Test Dell selection/copy, broad Unicode and wide-character alignment, wrapped-line selection and accessibility |
| Platform controls | C local socket checks hardware, BIOS, sound, input and guarded power | Keep controls usable across restart and cold boot; test physical input and audio quality |
| Releases | Sealed VM and Dell C releases with health gates and rollback; the VM look release passed checked reboot | Exercise delayed and failed startup recovery after reboot without losing root management |
| Independent control | SSH/watch/rescue recover failures after firmware hands off; a monitored Dell restart passed after restoring warning continuation | Build and physically prove an external power-button and observation path for firmware-logo stalls; keep local-recovery guard until then |
| Security | Root SSH keys, local peer-checked control socket, protected boot hashes | Reduce shared-X11/UID-1000 exposure and plan disk encryption with a recoverable boot path |
| Naming | Heurism C runtime and launchers live; older EFI, service, account and SSH names remain installed interfaces | Migrate remaining identifiers separately with exact boot and remote-access rollback evidence |

Wi-Fi association is deferred while Ethernet is the management link. The
direct USB Ethernet adapter has proven magic-packet wake from Linux `s2idle`;
Dell documents no Wake-on-LAN from full shutdown on this arrangement. Preserve
the SSD boot order, protected files, authenticated SSH/watch and legacy
recovery while iterating. Optional native kernel and USB/network drivers stay
isolated research, not deployment gates. See [independent control](independent-control.md)
for the missing preboot recovery layer and [workspace visual pass](heurism-look.md)
for the VM and Dell deployment evidence.

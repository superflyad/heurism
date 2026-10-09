# Heurism product roadmap

Heurism currently builds from Alpine packages and uses Xfce as the established
desktop. Its shell, terminal, settings, control and release verification are C. The
Dell is the primary physical target; the separate Hyper-V VM is the release
proving ground. See [architecture](architecture.md),
[current release](heurism-migration.md) and [security](security.md).

| Area | Current state | Next useful acceptance |
| --- | --- | --- |
| System base | Heurism identity on VM and Dell; VM package selection in `platform/heurism/vm-packages.list`; Alpine 3.24.2 package source retained | Build a fresh candidate image from the profile, boot it in isolation, and prove its upgrade and rollback path without losing management |
| Daily desktop | Xfce panel, workspace, Thunar, Mousepad, Firefox, Heurism Settings and Power | Complete ordinary file, browser and settings tasks from the Dell display with human input |
| Shell | C command runner with editing, history, pipes, redirection and basic globbing | Add job control, completion and more POSIX shell behavior with real PTY checks |
| Terminal | C X11/Xft/libvterm PTY with resize and Ctrl+C verified | Add scrollback, selection and clipboard before treating it as a primary terminal |
| Platform controls | C local socket checks hardware, BIOS, sound, input and guarded power | Keep controls usable across restart and cold boot; test physical input and audio quality |
| Releases | Sealed VM and Dell C releases with health gates and rollback; checked reboot passed | Exercise delayed and failed startup recovery after reboot without losing root management |
| Security | Root SSH keys, local peer-checked control socket, protected boot hashes | Reduce shared-X11/UID-1000 exposure and plan disk encryption with a recoverable boot path |
| Naming | Heurism C runtime and launchers live; older EFI, service, account and SSH names remain installed interfaces | Migrate remaining identifiers separately with exact boot and remote-access rollback evidence |

Wi-Fi association is deferred while Ethernet is the management link. The
direct USB Ethernet adapter has proven magic-packet wake from Linux `s2idle`;
Dell documents no Wake-on-LAN from full shutdown on this arrangement. Preserve
the SSD boot order, protected files, authenticated SSH/watch and legacy
recovery while iterating. Optional native kernel and USB/network drivers stay
isolated research, not deployment gates.

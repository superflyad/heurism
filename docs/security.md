# Heurism security status — 2026-10-08

## What is verified on the Dell

The last fully verified C release before the later
[terminal update and unverified reboot](dell-terminal-20261009.md) was
`/opt/heurism/native/releases/heurism-os-dell-20261009T020254Z-7718`.
Its checked C reboot returned boot `d5e85973-2087-4fc9-ab40-d43251868a3a` with
root SSH, watch, control and Xfce desktop healthy. The release manifest and six
protected EFI/kernel hashes passed. `BootCurrent` is `0005`, `BootOrder` is
`0005,0000`, `DriverOrder` is `0000,0001`, and `BootNext` is absent. These are
point-in-time checks, not continuous measured boot or whole-system integrity.

Root SSH uses public-key authentication with password login disabled. The
network listener audit found only `sshd` on port 22. The C control service
uses a local Unix socket with a restricted directory, `0660` socket mode and
`SO_PEERCRED` checks for root or `companion-ui`. It accepts named operations,
not arbitrary shell commands. This release rejects `admin-console` from the
desktop user; administrator work uses authenticated root SSH. The previous
root terminal in the shared X11 session is gone.

The Dell installer checks exact DMI, verifies sealed release files, activates
with a health gate and restores the prior release on failure. Its lock file
descriptor is closed in test and service children. A root installer staging
directory was found writable by UID 1000. It is now root-owned mode `0700` on
the Dell and VM, and both installers reject writable staged inputs. VM tests
proved they reject a writable file and a writable directory. This closes that
specific stage modification path; it does not prove earlier contents were
never modified. The existing SSD loader,
kernel, Ethernet management path and root SSH/watch services remain in place.
Xfce is supplied by Alpine packages. Its window manager, panel and file tools
make the desktop more usable; they do not add a security boundary. Heurism
Settings and the C terminal still run as UID 1000, and the C service still
checks local peer credentials. Xfce power manager autostart is disabled in the
user session so Heurism's checked power path remains the one used by its UI.
The active desktop and control OpenRC services are `heurism-desktop` and
`heurism-control`; root SSH and `companion-watch` stay independent.

## Limits that matter

- Xfce, Files, Editor, browser, Onboard and terminal share UID 1000 and
  one X11 session. X11 clients can observe or inject input and interact with
  other windows; a compromised client can read or change the user's files and
  can call the control socket's allowed privileged actions. Treat installed
  desktop programs and opened content as trusted for now.
- The root filesystem is unencrypted. Someone with physical access to the disk
  can read its data outside the running OS.
- The control socket still grants UID 1000 scoped hardware, BIOS, network and
  power actions. Peer credentials establish which Unix account connected, not
  which desktop application or human intended the action.
- The release hashes and power checks cover defined files and states. They do
  not establish secure boot, signed package updates, app sandboxing or recovery
  from every firmware failure.
- The C shell is a usable interactive command launcher with editing, session
  history and basic globbing. It is not a complete POSIX shell; BusyBox ash
  remains the script and recovery shell. Bounded graphical-terminal scrollback
  passed a live Dell test before the unverified reboot; clipboard and
  accessibility support remain incomplete.

## Next engineering gates

1. Make terminal scrollback, clipboard and window behavior dependable, then
   complete the shell interaction model, including job control and completion.
2. Split desktop privileges by application and require a separate, explicit
   administrator authentication path for sensitive actions. Replace the shared
   X11 trust boundary before treating untrusted desktop apps as isolated.
3. Establish an update and recovery policy with signed artifacts and a tested
   rollback path. Evaluate disk encryption with a recovery plan that preserves
   Dell access and the existing SSD management path.

Do not describe the current system as hardened or safe for untrusted desktop
software. The removal of the direct root-terminal action is a meaningful
improvement, but it does not create per-application isolation.

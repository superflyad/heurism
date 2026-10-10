# Heurism security status — 2026-10-09

The Dell runs Alpine Linux with the Heurism C control, shell, terminal and
release verifier, and an Xfce 4.20 session. This is a trusted-app desktop.
See [session security and updates](session-security-updates.md) for current
credentials, lock and update evidence.

## Verified on the physical Dell

- Root SSH remains pinned public-key-only (`PermitRootLogin
  prohibit-password`, `PasswordAuthentication no`). Root and the desktop
  account now have different, random nonempty local passwords. Recovery
  credentials are stored in owner-restricted Windows files under the ignored
  `artifacts/ssh` directory. Their contents are absent from the repository.
- The default Xfce session starts the PAM-backed `xfce4-screensaver` and locks
  before publishing desktop health. It locks after five idle minutes. Its
  process is monitored; killing it caused the old session to end and a new
  locked session to start. SSH and `companion-watch` remained healthy.
- Sealed C release
  `/opt/heurism/native/releases/heurism-os-dell-20261010T013332Z-20380`
  passed Dell sidecars and live release/health checks on boot
  `edb102d9-25c6-409c-aa17-181fea3eaf41`. The six protected EFI/kernel
  hashes match, BootCurrent is `0005`, BootOrder is `0005,0000`, DriverOrder
  is `0000,0001` and BootNext is absent. This release was activated by
  restarting only the graphical service; the Dell has not rebooted since.
- The local control socket accepts only named C actions and checks peer UID.
  Direct root terminals from the desktop are disabled. The installer seals
  release files and restores the prior C release if health activation fails.

## Limits

- Xfce, Firefox, Files, Editor, Onboard and the C terminal still share UID
  1000 and one X11 server. An app already running in this session can observe
  or inject input and interact with other apps. The lock protects ordinary
  physical walk-away access; it is not isolation from malicious same-user X11
  apps. The UID-1000 control socket can still perform its scoped privileged
  actions without separate administrator authentication.
- The disk is unencrypted. Physical access to its storage can bypass an OS
  password. The live session still starts automatically and immediately
  locks; this is not a display-manager login or a per-app boundary.
- Release hashes and package signatures are point-in-time checks, not secure
  boot or continuous whole-system integrity. The Dell has only one ext4 root
  partition and no independently tested full-OS rollback. Its earlier
  firmware-logo halt required a person to power-cycle it. Do not perform an
  unattended physical kernel, bootloader or partition update on this basis.

The VM has a tested host-checkpoint whole-system upgrade and rollback. The
fresh image build now generates per-image console and desktop passwords, and
boots with the desktop locked. Those VM results do not prove a safe Dell disk
migration or isolation of untrusted desktop apps.

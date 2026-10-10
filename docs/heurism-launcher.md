# Heurism C launcher

The VM's dock now opens a separate Heurism launcher window above ordinary
application windows. Its C/X11/Xft implementation is part of `heurism-desktop`;
it adds no guest runtime language or service. Search filters a fixed list of
Files, Editor, Browser, Terminal, Settings, System, Network, Keyboard and Power.
Arrow keys choose a result, Enter opens it, Escape closes the launcher, and
pointer clicks work on the rows. Settings, System, Network and Power open
ordinary Heurism C windows. Power still uses the existing checked controls;
opening its window performs no power operation. A second dock click raises the
existing launcher. The launcher is excluded from running-app task buttons.
The sealed desktop binary is 64,032 bytes, up from 59,752 bytes before this
launcher; the change adds no separate executable.

The preview at `build/prime-vm/heurism-launcher-preview.png` shows the launcher
with a Terminal search. `tests/native_launcher_vm.sh` runs an isolated
1280×800 Xvfb test against a sealed VM release. It clicked the dock,
searched for Terminal, opened it with Enter, opened Settings and Power through
search, then closed the launcher with Escape. It also checked the X11
skip-taskbar property and that repeated dock clicks leave one launcher window.

Before activation, checkpoint `Heurism-before-launcher-20261010` (UUID
`246de076-ca3f-401d-b398-01975e64d975`) preserved the VM. The guarded
installer assembled and activated release
`/opt/heurism/native/releases/heurism-os-20261010T145206Z-34622` after its
shell, interactive shell, app, control and release checks. Checked C reboot
returned fresh boot `66ee461d-3216-4c21-87ee-baadc68b3292` with that
release, a healthy UID-1000 workspace, Heurism window theme, SSH, watch and
control. Temporary build packages were removed. The ready VM checkpoint is
`Heurism-launcher-ready-20261010` (UUID
`080485cc-747e-4355-9f76-192703559050`). The Dell was not changed.

This is a focused launcher for known actions, not an index of installed apps
or files. The VM still uses Xfwm4 on Xorg, and the physical Dell remains on
its established Xfce desktop. The existing shared UID-1000 X11 and disk
security limits remain.

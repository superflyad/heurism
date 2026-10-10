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

## Keyboard access, 2026-10-10

The Heurism C workspace now grabs Super+Space while its session is running.
It opens or raises the same launcher from a focused application. The grab is
released with the C workspace process, so the Xfce recovery session's saved
keyboard shortcuts are unchanged. The workspace shows the shortcut beside its
other keyboard hints. If another client owns the grab, the desktop logs the
conflict and leaves the dock launcher available.

`tests/native_launcher_vm.sh` now activates a C Terminal, sends Super+Space
from that focused window, verifies the launcher, and then checks Escape and
the existing search actions. A first candidate exposed a focus timing error
when the launcher mapped. The final C code requests activation through the
window manager's existing X11 active-window route; the revised test passed.
The 1280×800 workspace capture at
`build/prime-vm/heurism-workspace-shortcut.png` shows the hint without clipping.

The prechange checkpoint is `Heurism-before-keyboard-launcher-20261010`
(UUID `14e5e9c1-f440-4d5c-97ae-f02cab75033c`). The guarded VM installer
activated sealed release
`/opt/heurism/native/releases/heurism-os-20261010T150201Z-40393` after its
existing C shell, app, control and release tests plus the isolated launcher
test. Checked C reboot returned fresh boot
`b822a784-1798-4585-8445-bcf25556e18b` with healthy C workspace, theme,
SSH, watch and control. No shortcut grab warning appeared in the live workspace
log. Build packages were removed. The ready checkpoint is
`Heurism-keyboard-launcher-ready-20261010` (UUID
`bb5c3d12-32ab-47aa-9a13-c30241b6f3d1`). The Dell was not changed.

# Heurism quick controls

The VM's C dock now makes its connection indicator clickable. It opens a
Heurism C quick-controls window beside the dock, above ordinary app windows.
The panel shows the actual Ethernet address and the VM's virtual-audio limit.
Its appearance button calls the existing checked C control service. Settings,
Network and Power open separate Heurism C windows. Power retains its two-click
confirmation before any restart or shutdown. Lock calls the session's Xfce
PAM screensaver. Escape and Close dismiss the panel; repeated dock clicks raise
the existing window. It stays out of the running-app task list.

The panel adds no executable or guest runtime language. The sealed C desktop
binary is 68,584 bytes, compared with 68,536 bytes in the preceding release.
The 1280×800 VM preview is
`build/prime-vm/heurism-quick-controls.png`. A separate light-preference
preview at `build/prime-vm/heurism-quick-controls-light.png` confirms its
dark panel text remains readable after the appearance toggle.

`tests/native_launcher_vm.sh` checks the dock click, one-panel reuse,
appearance change and restoration of the saved preference, Settings, Network,
Power and Escape. It also keeps the launcher and live-window checks. The
separate `tests/native_quick_lock_vm.sh` starts Xfwm and the real Xfce
screensaver together on an isolated VM display; clicking Quick Lock made the
screensaver report active. These tests passed for the sealed candidate and
again for the installed release after reboot. No power action was triggered
by the UI tests.

The prechange VM checkpoint is `Heurism-before-quick-controls-20261010`
(UUID `b3803c66-d1d6-4f8c-8a39-0ec365da9b2b`). The guarded installer
activated `/opt/heurism/native/releases/heurism-os-20261010T152041Z-51720`.
Checked C reboot returned fresh boot `e73fbbb1-9e62-4b4a-a528-04518fc8a8cf`
with the C desktop, theme, release verification, SSH, watch and control
healthy. The Night preference was restored after testing and temporary build
packages were removed. The ready checkpoint is
`Heurism-quick-controls-ready-20261010` (UUID
`749dc961-e3fa-47bf-87d3-ae5e9b5c50c7`). The Dell was not changed.

The quick panel is a VM-tested Heurism surface. Xfwm4 and Xorg still manage
windows, and the physical Dell remains on its established Xfce session.
Shared UID-1000 X11 and the unencrypted disk remain security limits.

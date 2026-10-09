# Xfce desktop bridge

The established desktop in Heurism is Alpine's Xfce 4.20 on the existing
Linux/Xorg foundation. It supplies window management, a top application panel,
a bottom launcher dock, desktop icons, Thunar file management, Mousepad editing
and an ordinary Xfce terminal. Heurism owned shell, terminal, settings,
control and release verification remain C programs. No boot loader, kernel,
Ethernet driver, root SSH service or watch service was replaced.

`userspace/native/user-session.sh` starts the user session after root has
configured Xorg and the platform. It applies saved Dell input settings, starts
the user audio server and launches `xfce4-session`. Health is published only
after `xfwm4`, `xfce4-panel`, `xfdesktop` and an X11 window manager are live.
The sealed `heurism-release` verifier checks that health against the current
release, boot ID, UID 1000 and a live Xfce session process. The native release
installer has an isolated Xvfb sidecar test and a rollback gate.

The application menu contains **Heurism Settings** and **Heurism C
Terminal**. The top-right red power icon and **Applications > System >
Heurism Power** open the C power window. They replace Xfce's session actions
button and disabled Log Out menu item. Heurism Power requires two clicks
within ten seconds and uses the checked C control socket. Settings is the C
platform control UI in a normal window; its
Sound, Input, Network, BIOS and checked Power pages still use the C root socket
service. Xfce power manager autostart is hidden for this session. The C terminal
opens the C shell through its own PTY. Thunar and Mousepad use the same UID-1000
home as other desktop apps. Firefox and Onboard remain Alpine applications.

For recovery, a root administrator can place the single word `native` in
`/etc/companion/native-session-mode` and restart `companion-desktop`; this
selects the original C workspace and dock. Removing the file restores Xfce.
Use the VM installer on `CompanionDev` and the exact-DMI guarded Dell installer
for versioned releases. Root SSH and `companion-watch` are independent of the
session in either mode. The sealed Python/Tk release remains a deeper fallback.
The active renamed releases and checked reboot evidence are recorded in
[heurism-migration.md](heurism-migration.md). The evidence below is the earlier
Xfce bridge baseline under its installed names.

## Verification on 2026-10-03

- The VM Xfce session, Companion Settings, Thunar, Mousepad and C terminal
  passed an isolated Xvfb test. The VM checked reboot returned painted Xfce on
  fresh boot `43dc3475-3d7a-4947-84c0-d5aadf600973`; the final UI candidate
  was activated under the rollback gate and its health verified.
  Final VM state was saved as checkpoint `Companion-Xfce-bridge-final-20261003`
  (UUID `8cf15a9d-b3ca-4569-a098-bf0681ff2b4c`).
- The Dell candidate passed the installer sidecar suite, including the C shell,
  interactive PTY, control socket, Dell hardware, X11 apps and bad-manifest
  rollback. The live Dell session launched Thunar and Mousepad, opened Sound
  and Input in Companion Settings, and closed Settings without taking down the
  desktop. A checked reboot of the final release returned painted Xfce on
  fresh boot `620b5571-88de-46b4-9576-0c43cbfc737d` with SSH, watch, control,
  UI and protected boot files healthy. The default SSD boot order was restored
  after firmware appended its known NIC entries.
- The power update passed isolated Xfce tests and guarded activation on VM and
  Dell. A live Dell UI click opened Power and showed both confirmation states;
  its confirmed Restart returned fresh boot
  `2fe46192-7c97-4629-97f9-757032debc23`, with root SSH, watch, control,
  Xfce and protected boot files healthy. The VM completed both UI Restart and
  UI Shut down; Hyper-V observed it Off, then host start returned fresh boot
  `d383719e-9de0-4f3b-9886-9e29e522e0f9`.
  The current VM checkpoint is `Companion-Xfce-power-menu-ready-20261003`
  (UUID `a3d74a06-adf2-4523-bb5c-184d857dba03`).

Xfce adds many Alpine packages and a fuller desktop experience. It does not
isolate applications: all desktop clients still share UID 1000 and one X11
session. The root filesystem is still unencrypted. See [security.md](security.md).

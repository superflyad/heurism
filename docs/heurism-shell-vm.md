# Heurism C workspace on the development VM

The `CompanionDev` VM now selects `heurism` in
`/etc/companion/native-session-mode`. The visible desktop and dock are drawn by
`heurism-desktop` in C/X11/Xft. Its workspace opens the C Files, Editor and
Terminal, Firefox, network/settings pages, and running window tasks. Xfwm4
still manages windows and Xfce's PAM screensaver locks the session. Alpine
Linux remains the kernel, driver, package and Unix service foundation. The
physical Dell remains on its proven Xfce session.

The VM release is
`/opt/heurism/native/releases/heurism-os-20261010T142713Z-21679`.
Its first checked C reboot returned boot
`0e0739b9-f3b1-4692-b562-2120b64026ca` with the same C workspace,
release verification, checked power, protected hashes and SSH/watch/control
services healthy. Xfce's panel and desktop processes are absent in this mode.
The VM checkpoint before changing the session is
`Heurism-C-workspace-experiment-20261010` (UUID
`f0db5c40-16dc-4800-9ca5-35aca8b363fe`).

An isolated 1280×800 Xvfb session captured the rendered workspace and opened
the real C Files and Terminal windows through pointer clicks. Both appeared
as X11 windows and in the task dock. The active VM session uses a password
lock, so its framebuffer is intentionally black before unlock; Xvfb evidence
does not substitute for physical Dell display or touch acceptance.

The new `heurism` mode starts Xfwm4, Xfce settings and the PAM screensaver,
verifies that the screen is locked, then starts the C workspace. If the
workspace or locker exits, the root session client atomically selects `xfce`
for the next supervised start. In a live VM fault test, terminating the
locker switched the mode to `xfce`; a locked Xfce session, panel, desktop,
SSH, watch and C control all returned on the same boot. Re-selecting
`heurism` and restarting the desktop restored the C workspace. The VM's
single-root rollback checkpoint and host reset remain available.

The final release also creates its root-owned state directory before loading
preferences. The previous VM release could not save appearance because that
directory was missing. Light and night settings now save to a mode-0600 file;
night persisted across the final checked reboot. Temporary C build packages
were removed afterward.

This is a meaningful desktop change, not a completed Dell migration. Xfwm4
window frames and the Xfce screensaver are upstream components; GTK apps
retain their current themes. The shared X11 UID-1000 session and unencrypted
disk remain security limits. The fresh A/B image prototype still defaults to
Xfce and does not stage signed updates into its inactive root. Before a Dell
switch, the C session needs direct keyboard/touch/accessibility acceptance,
window and app reliability checks, stronger theme integration, a signed
whole-system update path, and a credible recovery route for a firmware-logo
halt that cannot be fixed through SSH. Do not change the Dell boot path or
desktop mode on this VM evidence alone.

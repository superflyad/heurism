# Heurism C workspace on the Dell, isolated display gate

On 2026-10-10, the pinned Dell SSH connection reached boot
`edb102d9-25c6-409c-aa17-181fea3eaf41`. The installed release remained
`/opt/heurism/native/releases/heurism-os-dell-20261010T013332Z-20380` and the
production user session remained Xfce on Xorg `:0`.

The VM's sealed `heurism-desktop` 0.6 binary was copied as an inert test input
to the Dell staging directory. Its SHA256 was
`ec404f0bbf1c0cff3639128507e9e5546865afca0b14cdecc97bc19ebe8e5e0e` on
both the VM and Dell. The current Heurism Xfwm theme archive was supplied
separately because the installed Dell release predates that asset.

`tests/native_workspace_dell.sh` started a private `1920x1080` Xvfb `:93`
with an X11 authorization cookie, isolated user home/configuration and Xfwm4.
It ran the candidate C workspace as `companion-ui` and checked the dock,
quick-controls panel, skip-taskbar state, launcher, terminal launch, and
Super+Space while a terminal was focused. It captured the actual rendered
workspace and quick panel. The quick panel showed the Dell Ethernet address
`10.8.22.238` and offered speaker controls through Settings. The captures are
in `build/prime-vm/heurism-dell-workspace.png` and
`build/prime-vm/heurism-dell-quick.png` on the development host.

The test passed after a clean rerun. The active release link, boot ID and
production `:0` session were unchanged; `heurism-desktop`, `heurism-control`,
`companion-watch` and `sshd` remained started. The eight protected hash checks
passed. No Dell reboot, boot variable change, desktop mode change, or physical
touch input was involved.

This gate proves the C workspace can render and operate in an isolated Dell-size
X11 session. It does not prove that the physical screen, touchscreen, keyboard,
audio, password lock, session failure fallback, and recovery all work together
after selecting `heurism` as the Dell default. Before that switch, assemble a
complete Dell release containing the new theme and session scripts, run its
guarded rollback checks, then validate the actual `:0` session with local
recovery available. A firmware-logo halt still cannot be repaired over SSH.

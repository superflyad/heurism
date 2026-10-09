#!/bin/sh
set -eu
export DISPLAY=:0
export XAUTHORITY=/run/companion-desktop/Xauthority
mkdir -p /run/companion-desktop
python3 /opt/companion/desktop/session-config.py
umask 077
cookie=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
touch "$XAUTHORITY"
xauth -f "$XAUTHORITY" add :0 . "$cookie"
chown root:companion-ui "$XAUTHORITY"
chmod 640 "$XAUTHORITY"
exec xinit /opt/companion/desktop/client.sh -- /usr/bin/Xorg :0 vt7 \
    -config /run/companion-desktop/xorg.conf -nolisten tcp -auth "$XAUTHORITY" \
    -logfile /var/log/companion-xorg.log

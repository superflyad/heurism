#!/bin/sh
openbox --config-file /opt/companion/desktop/openbox.xml > "$HOME/.local/state/companion/openbox.log" 2>&1 &
wm=$!
pulseaudio --start --exit-idle-time=-1 >> "$HOME/.local/state/companion/audio.log" 2>&1 || true
python3 /opt/companion/desktop/shell.py
result=$?
kill "$wm" 2>/dev/null || true
wait "$wm" 2>/dev/null || true
exit "$result"

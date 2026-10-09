#!/bin/sh
# Replace Xfce's unavailable session power actions with Heurism's checked UI.
set -eu
release=${HEURISM_RELEASE:-/opt/heurism/native/current}
panel=1
ids=$(xfconf-query -c xfce4-panel -p /panels/panel-1/plugin-ids |
    sed -n '/^[0-9][0-9]*$/p')
test -n "$ids"
legacy=
for id in $ids; do
    items=$(xfconf-query -c xfce4-panel -p "/plugins/plugin-$id/items" 2>/dev/null || true)
    if printf '%s\n' "$items" | grep -Fxq heurism-power.desktop; then
        exit 0
    fi
    if printf '%s\n' "$items" | grep -Fxq companion-power.desktop; then
        legacy=$id
    fi
done
id=19
while test -e "$HOME/.config/xfce4/panel/launcher-$id" ||
      xfconf-query -c xfce4-panel -p "/plugins/plugin-$id" >/dev/null 2>&1; do
    id=$((id + 1))
done
directory="$HOME/.config/xfce4/panel/launcher-$id"
mkdir -p "$directory"
cp "$release/heurism-power.desktop" "$directory/heurism-power.desktop"
xfconf-query -c xfce4-panel -p "/plugins/plugin-$id" -n -t string -s launcher
xfconf-query -c xfce4-panel -p "/plugins/plugin-$id/items" -n -a \
    -t string -s heurism-power.desktop
set --
for old in $ids; do
    kind=$(xfconf-query -c xfce4-panel -p "/plugins/plugin-$old" 2>/dev/null || true)
    if [ "$kind" != actions ] && [ "$old" != "$legacy" ]; then
        set -- "$@" -t int -s "$old"
    fi
done
set -- "$@" -t int -s "$id"
xfconf-query -c xfce4-panel -p "/panels/panel-$panel/plugin-ids" "$@"
if [ -n "$legacy" ] && [ -f /opt/companion/native/current/companion-power.desktop ]; then
    cp /opt/companion/native/current/companion-power.desktop \
        "$HOME/.config/xfce4/panel/launcher-$legacy/companion-power.desktop"
fi
xfce4-panel -r

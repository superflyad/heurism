#!/bin/sh
# Replace Xfce's unavailable session power actions with Companion's checked UI.
set -eu
release=${COMPANION_RELEASE:-/opt/companion/native/current}
panel=1
ids=$(xfconf-query -c xfce4-panel -p /panels/panel-1/plugin-ids |
    sed -n '/^[0-9][0-9]*$/p')
test -n "$ids"
for id in $ids; do
    if xfconf-query -c xfce4-panel -p "/plugins/plugin-$id/items" 2>/dev/null |
        grep -Fxq companion-power.desktop; then
        exit 0
    fi
done
id=19
while test -e "$HOME/.config/xfce4/panel/launcher-$id" ||
      xfconf-query -c xfce4-panel -p "/plugins/plugin-$id" >/dev/null 2>&1; do
    id=$((id + 1))
done
directory="$HOME/.config/xfce4/panel/launcher-$id"
mkdir -p "$directory"
cp "$release/companion-power.desktop" "$directory/companion-power.desktop"
xfconf-query -c xfce4-panel -p "/plugins/plugin-$id" -n -t string -s launcher
xfconf-query -c xfce4-panel -p "/plugins/plugin-$id/items" -n -a \
    -t string -s companion-power.desktop
set --
for old in $ids; do
    kind=$(xfconf-query -c xfce4-panel -p "/plugins/plugin-$old" 2>/dev/null || true)
    test "$kind" = actions || set -- "$@" -t int -s "$old"
done
set -- "$@" -t int -s "$id"
xfconf-query -c xfce4-panel -p "/panels/panel-$panel/plugin-ids" "$@"
xfce4-panel -r

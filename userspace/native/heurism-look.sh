#!/bin/sh
# Apply the Heurism workspace once; retain later user panel choices.
set -eu
release=${HEURISM_RELEASE:-/opt/heurism/native/current}
test -r "$release/heurism-wallpaper.svg"
test -r "$release/heurism-mark.svg"
config="$HOME/.config/heurism"
mkdir -p "$config"

monitor=$(xrandr --listmonitors 2>/dev/null | awk 'NR==2 {print $NF}')
case "$monitor" in
    ''|*[!A-Za-z0-9_-]*) echo 'No supported Xfce monitor name' >&2 ;;
    *)
        backdrop=/backdrop/screen0/monitor${monitor}/workspace0
        image=$(xfconf-query -c xfce4-desktop -p "$backdrop/last-image" 2>/dev/null || true)
        case "$image" in
            ''|/usr/share/heurism/wallpaper.svg|/usr/share/backgrounds/xfce/*|\
            /opt/heurism/native/current/heurism-wallpaper.svg|\
            /opt/heurism/native/releases/*/heurism-wallpaper.svg)
                xfconf-query -c xfce4-desktop -p "$backdrop/last-image" -n -t string \
                    -s "$release/heurism-wallpaper.svg"
                xfconf-query -c xfce4-desktop -p "$backdrop/image-style" -n -t int -s 5
                xfdesktop --reload || true ;;
        esac ;;
esac

if [ ! -e "$config/look-v1" ]; then
    panel=xfce4-panel
    xfconf-query -c "$panel" -p /panels/panel-1/size -n -t uint -s 36
    xfconf-query -c "$panel" -p /panels/panel-1/icon-size -n -t uint -s 22
    xfconf-query -c "$panel" -p /panels/panel-1/background-style -n -t uint -s 1
    xfconf-query -c "$panel" -p /panels/panel-1/background-rgba -n -a \
        -t double -s 0.035 -t double -s 0.067 -t double -s 0.118 -t double -s 0.97
    xfconf-query -c "$panel" -p /panels/panel-2/size -n -t uint -s 56
    xfconf-query -c "$panel" -p /panels/panel-2/background-style -n -t uint -s 1
    xfconf-query -c "$panel" -p /panels/panel-2/background-rgba -n -a \
        -t double -s 0.055 -t double -s 0.11 -t double -s 0.16 -t double -s 0.97
    ids=$(xfconf-query -c "$panel" -p /panels/panel-1/plugin-ids |
        sed -n '/^[0-9][0-9]*$/p')
    for id in $ids; do
        kind=$(xfconf-query -c "$panel" -p "/plugins/plugin-$id" 2>/dev/null || true)
        if [ "$kind" = applicationsmenu ]; then
            xfconf-query -c "$panel" -p "/plugins/plugin-$id/button-title" -n -t string -s Heurism
            xfconf-query -c "$panel" -p "/plugins/plugin-$id/show-button-title" -n -t bool -s true
            xfconf-query -c "$panel" -p "/plugins/plugin-$id/button-icon" -n -t string \
                -s "$release/heurism-mark.svg"
            break
        fi
    done
    icon_style=$(xfconf-query -c xfce4-desktop -p /desktop-icons/style 2>/dev/null || true)
    case "$icon_style" in
        ''|2) xfconf-query -c xfce4-desktop -p /desktop-icons/style -n -t int -s 0 ;;
    esac
    printf '1\n' >"$config/look-v1"
fi

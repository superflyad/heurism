"""Return to the interactive Companion desktop without Openbox's input-only overlay."""
import re
import subprocess


def main():
    listing = subprocess.check_output(['xprop', '-root', '_NET_CLIENT_LIST'], text=True, timeout=2)
    desktop = None
    for window in re.findall(r'0x[0-9a-fA-F]+', listing):
        try:
            kind = subprocess.check_output(['xprop', '-id', window, '_NET_WM_WINDOW_TYPE'], text=True, timeout=2)
            if '_NET_WM_WINDOW_TYPE_DESKTOP' in kind:
                desktop = window
            elif '_NET_WM_WINDOW_TYPE_DOCK' not in kind:
                subprocess.run(['xdotool', 'windowminimize', window], timeout=2)
        except subprocess.SubprocessError:
            continue
    if desktop:
        subprocess.run(['xdotool', 'windowfocus', desktop, 'key', 'F4'], timeout=2)


if __name__ == '__main__':
    main()

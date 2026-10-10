"""Host-only generator for Heurism's small Xfwm window-decoration assets."""

import gzip
import io
import tarfile
from pathlib import Path

from PIL import Image, ImageDraw


OUTPUT = Path(__file__).resolve().parents[1] / "userspace/native/heurism-xfwm4.tar.gz"
ACTIVE = (12, 32, 43, 255)
INACTIVE = (28, 43, 53, 255)
ACCENT = (77, 211, 194, 255)
WARM = (215, 168, 101, 255)
TEXT = (239, 245, 255, 255)
MUTED = (146, 166, 178, 255)


def png(image):
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=True)
    return output.getvalue()


def image(width, height, color):
    return Image.new("RGBA", (width, height), color)


def frame_piece(name, active):
    color = ACTIVE if active else INACTIVE
    edge = ACCENT if active else MUTED
    if name.startswith("title-"):
        result = image(8, 34, color)
        ImageDraw.Draw(result).rectangle((0, 32, 7, 33), fill=edge)
    elif name.startswith("top-"):
        result = image(8, 34, color)
        draw = ImageDraw.Draw(result)
        draw.rectangle((0, 32, 7, 33), fill=edge)
        if "left" in name:
            draw.rectangle((0, 4, 1, 33), fill=edge)
            draw.rectangle((0, 0, 3, 1), fill=edge)
        else:
            draw.rectangle((6, 4, 7, 33), fill=edge)
            draw.rectangle((4, 0, 7, 1), fill=edge)
    elif name in ("left", "right"):
        result = image(5, 24, color)
        draw = ImageDraw.Draw(result)
        draw.rectangle((0 if name == "left" else 3, 0,
                        1 if name == "left" else 4, 23), fill=edge)
    elif name == "bottom":
        result = image(24, 5, color)
        ImageDraw.Draw(result).rectangle((0, 3, 23, 4), fill=edge)
    else:
        result = image(16, 16, color)
        draw = ImageDraw.Draw(result)
        draw.rectangle((0, 14, 15, 15), fill=edge)
        draw.rectangle((0 if "left" in name else 14, 0,
                        1 if "left" in name else 15, 15), fill=edge)
    return result


def button(kind, state, toggled=False):
    active = state != "inactive"
    background = (30, 69, 75, 255) if state == "prelight" else (
        (42, 87, 90, 255) if state == "pressed" else ACTIVE if active else INACTIVE)
    width = 22 if kind == "menu" else 21
    result = image(width, 34, background)
    draw = ImageDraw.Draw(result)
    color = MUTED if not active else WARM if kind == "close" else TEXT
    if state == "prelight":
        color = ACCENT
    if state == "pressed":
        color = (8, 32, 39, 255)
    center = width // 2
    if kind == "close":
        draw.line((center - 4, 13, center + 4, 21), fill=color, width=2)
        draw.line((center + 4, 13, center - 4, 21), fill=color, width=2)
    elif kind == "hide":
        draw.line((center - 5, 20, center + 5, 20), fill=color, width=2)
    elif kind == "maximize":
        draw.rectangle((center - 5, 12, center + 5, 22), outline=color, width=2)
        if toggled:
            draw.line((center - 3, 15, center + 3, 15), fill=color, width=2)
    elif kind == "menu":
        for row in (13, 17, 21):
            draw.line((center - 5, row, center + 5, row), fill=color, width=2)
    elif kind == "shade":
        points = ((center - 4, 18, center, 14, center + 4, 18) if toggled else
                  (center - 4, 15, center, 19, center + 4, 15))
        draw.line(points, fill=color, width=2)
    else:  # stick
        draw.ellipse((center - 4, 13, center + 4, 21), outline=color, width=2)
        if toggled:
            draw.ellipse((center - 1, 16, center + 1, 18), fill=color)
    return result


def main():
    files = {
        "themerc": b"active_text_color=#eff5ff\n"
                    b"inactive_text_color=#92a6b2\n"
                    b"button_offset=4\nbutton_spacing=3\n"
                    b"full_width_title=true\nshow_app_icon=false\n"
                    b"title_horizontal_offset=12\n"
                    b"title_shadow_active=false\ntitle_shadow_inactive=false\n"
    }
    edges = ["top-left", "top-right", "left", "right", "bottom",
             "bottom-left", "bottom-right"]
    edges += [f"title-{index}" for index in range(1, 6)]
    for active in (True, False):
        suffix = "active" if active else "inactive"
        for edge in edges:
            files[f"{edge}-{suffix}.png"] = png(frame_piece(edge, active))
    for kind in ("close", "hide", "maximize", "menu", "shade", "stick"):
        for state in ("active", "inactive", "prelight", "pressed"):
            files[f"{kind}-{state}.png"] = png(button(kind, state))
            if kind in ("maximize", "shade", "stick"):
                files[f"{kind}-toggled-{state}.png"] = png(button(kind, state, True))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as archive:
                for name, data in sorted(files.items()):
                    info = tarfile.TarInfo(name)
                    info.size = len(data)
                    info.mode = 0o644
                    info.mtime = 0
                    archive.addfile(info, io.BytesIO(data))
    print(f"{OUTPUT}: {len(files)} files, {OUTPUT.stat().st_size} bytes")


if __name__ == "__main__":
    main()

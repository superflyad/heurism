"""Host-only generator for Heurism's small Xfwm window-decoration assets."""

import gzip
import io
import tarfile
from pathlib import Path

from PIL import Image, ImageDraw


OUTPUT = Path(__file__).resolve().parents[1] / "userspace/native/heurism-xfwm4.tar.gz"
ACTIVE = (25, 35, 52, 255)
INACTIVE = (29, 40, 57, 255)
ACCENT = (77, 211, 194, 255)
WARM = (225, 151, 144, 255)
TEXT = (239, 245, 255, 255)
MUTED = (146, 166, 184, 255)


def png(image):
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=True)
    return output.getvalue()


def image(width, height, color):
    return Image.new("RGBA", (width, height), color)


def frame_piece(name, active):
    color = ACTIVE if active else INACTIVE
    edge = (56, 73, 92, 255) if active else (42, 56, 73, 255)
    if name.startswith("title-"):
        result = image(8, 34, color)
        ImageDraw.Draw(result).rectangle((0, 33, 7, 33), fill=edge)
    elif name.startswith("top-"):
        result = image(8, 34, color)
        draw = ImageDraw.Draw(result)
        draw.rectangle((0, 33, 7, 33), fill=edge)
        if "left" in name:
            draw.rectangle((0, 4, 0, 33), fill=edge)
            draw.rectangle((0, 0, 3, 0), fill=edge)
        else:
            draw.rectangle((7, 4, 7, 33), fill=edge)
            draw.rectangle((4, 0, 7, 0), fill=edge)
    elif name in ("left", "right"):
        result = image(5, 24, color)
        draw = ImageDraw.Draw(result)
        draw.rectangle((0 if name == "left" else 4, 0,
                        0 if name == "left" else 4, 23), fill=edge)
    elif name == "bottom":
        result = image(24, 5, color)
        ImageDraw.Draw(result).rectangle((0, 4, 23, 4), fill=edge)
    else:
        result = image(16, 16, color)
        draw = ImageDraw.Draw(result)
        draw.rectangle((0, 15, 15, 15), fill=edge)
        draw.rectangle((0 if "left" in name else 15, 0,
                        0 if "left" in name else 15, 15), fill=edge)
    return result


def button(kind, state, toggled=False):
    active = state != "inactive"
    background = (47, 65, 83, 255) if state == "prelight" else (
        (61, 81, 100, 255) if state == "pressed" else ACTIVE if active else INACTIVE)
    width = 22 if kind == "menu" else 21
    result = image(width, 34, background)
    draw = ImageDraw.Draw(result)
    color = MUTED if not active else TEXT
    if state == "prelight":
        color = WARM if kind == "close" else ACCENT
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
                    b"inactive_text_color=#92a6b8\n"
                    b"button_layout=|HMC\nbutton_offset=4\nbutton_spacing=3\n"
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

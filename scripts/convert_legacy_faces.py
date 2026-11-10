#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-off conversion of the EleksTubeHAX clock faces (data/<face><digit>.bmp,
names in data/clockfaces.txt) into Tube Clock image sets (<set>_0_<digit>.png).

Usage: python3 scripts/convert_legacy_faces.py <legacy-data-dir> <output-dir>
"""
import pathlib
import re
import sys

from PIL import Image

WIDTH, HEIGHT = 135, 240


def set_name(title: str) -> str:
    name = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
    return name[:20].rstrip("-")


def save_png(img: Image.Image, path: pathlib.Path) -> None:
    img = img.convert("RGB")
    colors = img.getcolors(256)
    if colors is not None:  # <= 256 colours: lossless palette image
        img = img.quantize(colors=len(colors), method=Image.Quantize.MAXCOVERAGE)
    img.save(path, format="PNG", optimize=True)


def main() -> None:
    src, dst = map(pathlib.Path, sys.argv[1:3])
    dst.mkdir(parents=True, exist_ok=True)
    titles = (src / "clockfaces.txt").read_text().splitlines()
    total = 0
    for face, title in enumerate(titles, start=1):
        name = set_name(title)
        for digit in range(10):
            bmp = src / f"{face}{digit}.bmp"
            img = Image.open(bmp).convert("RGB")
            if img.width > WIDTH or img.height > HEIGHT:
                raise SystemExit(f"{bmp}: larger than {WIDTH}x{HEIGHT}: {img.size}")
            if img.size != (WIDTH, HEIGHT):
                # The old firmware centred smaller images on black; bake that in.
                canvas = Image.new("RGB", (WIDTH, HEIGHT))
                canvas.paste(img, ((WIDTH - img.width) // 2, (HEIGHT - img.height) // 2))
                img = canvas
            out = dst / f"{name}_0_{digit}.png"
            save_png(img, out)
            total += out.stat().st_size
        print(f"face {face} '{title}' -> {name}")
    print(f"total {total / 1024:.0f} KiB")


if __name__ == "__main__":
    main()

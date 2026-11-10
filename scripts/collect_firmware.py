#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Collects the built firmware of every clock model into a bundle for the desktop app.

    pio run -e EleksTube -e EleksTube_Gen2 -e SI_HAI -e PunkCyber
    python3 scripts/collect_firmware.py [--out dist/firmware]

Output layout (installed by the app under share/tubeclock/firmware):

    <out>/manifest.json
    <out>/<model>/bootloader.bin, app.bin           one build per model
    <out>/layouts/<size>/partitions.bin             one partition table per flash size

The firmware is built once (for 4 MB). Clocks with more flash use the same boot loader and
app: the desktop app sets the flash size in their image headers (and recomputes the
image hash) and writes the partition table of the matching layout. manifest.json lists
all offsets, so the app never hard-codes the flash layout.
"""
import argparse
import csv
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODELS = {
    "EleksTube": "EleksTube IPS (original)",
    "EleksTube_Gen2": "EleksTube IPS Gen2 / Classic / Pro / PR1 / PR2",
    "SI_HAI": "Si Hai IPS clock",
    "PunkCyber": "PunkCyber / RGB Glow Tube / PCBway",
}
# Flash size -> (partition CSV, size code in the ESP32 image header)
LAYOUTS = {
    "4MB": ("partition_4MB.csv", 2),
    "8MB": ("partition_8MB.csv", 3),
    "16MB": ("partition_16MB.csv", 4),
}
BOOTLOADER_OFFSET = 0x1000
PARTITION_TABLE_OFFSET = 0x8000
FRAMEWORK = pathlib.Path.home() / ".platformio" / "packages" / "framework-arduinoespressif32"


def firmware_version() -> str:
    text = (ROOT / "platformio.ini").read_text()
    m = re.search(r"-D\s*BUILDVER=([0-9A-Za-z.\-]+)", text)
    if not m:
        raise SystemExit("BUILDVER not found in platformio.ini")
    return m.group(1)


def partitions(csv_path: pathlib.Path) -> dict:
    result = {}
    with csv_path.open() as f:
        for row in csv.reader(line for line in f if not line.lstrip().startswith("#")):
            if len(row) < 5:
                continue
            name, _type, _subtype, offset, size = (c.strip() for c in row[:5])
            result[name] = {"offset": int(offset, 0), "size": int(size, 0)}
    return result


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_partition_table(csv_path: pathlib.Path, flash_size: str, out: pathlib.Path) -> None:
    tool = FRAMEWORK / "tools" / "gen_esp32part.py"
    if not tool.exists():
        raise SystemExit(f"{tool} not found - build a firmware environment first")
    subprocess.run([sys.executable, str(tool), "-q", "--flash-size", flash_size, str(csv_path), str(out)],
                   check=True)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "dist" / "firmware")
    args = ap.parse_args()
    version = firmware_version()

    if args.out.exists():
        shutil.rmtree(args.out)
    args.out.mkdir(parents=True)
    manifest = {"firmware_version": version, "chip": "esp32", "layouts": {}, "models": {}}

    app_area = None
    for size, (csv_name, size_code) in LAYOUTS.items():
        table = partitions(ROOT / csv_name)
        if "otadata" in table:
            raise SystemExit(f"{csv_name}: OTA layouts are not supported (boot_app0 is never written)")
        if app_area is None:
            app_area = table["app0"]
        elif table["app0"] != app_area:
            raise SystemExit(f"{csv_name}: the app partition must be the same in every layout")
        out = args.out / "layouts" / size
        out.mkdir(parents=True)
        build_partition_table(ROOT / csv_name, size, out / "partitions.bin")
        manifest["layouts"][size] = {
            "flash_size": int(size[:-2]) * 1024 * 1024,
            "header_size_code": size_code,
            "partitions": {
                "file": f"layouts/{size}/partitions.bin",
                "offset": PARTITION_TABLE_OFFSET,
                "sha256": sha256(out / "partitions.bin"),
            },
            "filesystem": {
                "type": "littlefs",
                "offset": table["littlefs"]["offset"],
                "size": table["littlefs"]["size"],
                "block_size": 4096,
            },
        }
        print(f"layout {size}: ok")

    for model, title in MODELS.items():
        build = ROOT / ".pio" / "build" / model
        app = build / f"{model}_v{version}.bin"
        if not app.exists():
            raise SystemExit(f"{app} missing - run: pio run -e {model}")
        if app.stat().st_size > app_area["size"]:
            raise SystemExit(f"{model}: app.bin does not fit the app partition")
        out = args.out / model
        out.mkdir()
        images = []
        for name, src, offset in (("bootloader.bin", build / "bootloader.bin", BOOTLOADER_OFFSET),
                                  ("app.bin", app, app_area["offset"])):
            shutil.copy2(src, out / name)
            images.append({"file": f"{model}/{name}", "offset": offset, "sha256": sha256(out / name)})
        manifest["models"][model] = {"title": title, "images": images}
        print(f"{model}: ok")

    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"bundle written to {args.out}")


if __name__ == "__main__":
    main()

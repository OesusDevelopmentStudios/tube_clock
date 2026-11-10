#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generates packaging/flatpak/python3-esptool.json (pinned esptool + dependencies).

esptool is installed without `cryptography` (needs Rust to build and is only used
by espsecure/secure boot features the app never calls) and with bitstring < 4.4
(newer releases depend on the Rust extension `tibs`).

Usage: python3 scripts/gen_flatpak_python_deps.py
"""
import json
import pathlib
import urllib.request

PACKAGES = {
    "esptool": "5.2.0",
    "pyserial": "3.5",
    "intelhex": "2.3.0",
    "PyYAML": "6.0.3",
    "reedsolo": "1.7.0",
    "bitarray": "3.12.3",
    "bitstring": "4.3.1",
    "click": "8.5.1",
    "rich-click": "1.9.9",
    "rich": "15.0.0",
    "markdown-it-py": "4.2.0",
    "mdurl": "0.1.2",
    "Pygments": "2.21.0",
    "typing_extensions": "4.15.0",
}

OUT = pathlib.Path(__file__).resolve().parent.parent / "packaging" / "flatpak" / "python3-esptool.json"


def pick(name: str, version: str) -> dict:
    with urllib.request.urlopen(f"https://pypi.org/pypi/{name}/{version}/json", timeout=30) as r:
        files = json.load(r)["urls"]
    # Prefer pure Python wheels (no compilation); otherwise the source archive.
    for f in files:
        if f["packagetype"] == "bdist_wheel" and f["filename"].endswith("-py3-none-any.whl"):
            return f
    for f in files:
        if f["packagetype"] == "bdist_wheel" and "none-any" in f["filename"]:
            return f
    for f in files:
        if f["packagetype"] == "sdist":
            return f
    raise SystemExit(f"no usable file for {name} {version}")


def main() -> None:
    sources = []
    names = []
    for name, version in PACKAGES.items():
        f = pick(name, version)
        sources.append({"type": "file", "url": f["url"], "sha256": f["digests"]["sha256"]})
        names.append(f"{name}=={version}")
        print(f"{name} {version}: {f['filename']}")
    module = {
        "name": "python3-esptool",
        "buildsystem": "simple",
        "build-commands": [
            "pip3 install --verbose --exists-action=i --no-index --find-links=\"file://${PWD}\" "
            "--prefix=${FLATPAK_DEST} --no-build-isolation --no-deps " + " ".join(f'"{n}"' for n in names)
        ],
        "sources": sources,
    }
    OUT.write_text(json.dumps(module, indent=2) + "\n")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()

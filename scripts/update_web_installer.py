#!/usr/bin/env python3
"""Prepare and stage the PocketDeck-OS web installer (installer/).

The installer page flashes the X3 from Chrome or Edge with ESP Web Tools.
It writes the same four images that `pio run -e default -t upload` writes:

  0x0000   bootloader.bin
  0x8000   partitions.bin
  0xe000   boot_app0.bin  (otadata: boot the first app slot)
  0x10000  pocketdeck-os-x3.bin  (the app)

Usage:
  # After `pio run -e default`: copy the new build into dist/ and installer/,
  # and write installer/firmware/x3/manifest.json with the current version.
  python3 scripts/update_web_installer.py

  # Assemble the full static site in DIR (used by the Pages workflow and for
  # local preview). The app image is copied from dist/ so it is not stored twice.
  python3 scripts/update_web_installer.py --stage DIR
"""

import argparse
import configparser
import datetime
import json
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
INSTALLER = REPO / "installer"
X3_DIR = INSTALLER / "firmware" / "x3"
DIST_APP = REPO / "dist" / "pocketdeck-os-x3.bin"
BUILD = REPO / ".pio" / "build" / "default"
BOOT_APP0 = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
APP_NAME = "pocketdeck-os-x3.bin"


def read_version() -> str:
    config = configparser.ConfigParser(inline_comment_prefixes=(";",), strict=False, interpolation=None)
    config.read(REPO / "platformio.ini")
    return config.get("crossink", "pocketdeck_version", fallback="dev").strip()


def require(path: Path) -> Path:
    if not path.is_file():
        sys.exit(f"error: missing {path.relative_to(REPO) if path.is_relative_to(REPO) else path}. "
                 "Run `pio run -e default` first.")
    return path


def prepare() -> None:
    app = require(BUILD / "firmware.bin")
    X3_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(app, DIST_APP)
    shutil.copyfile(require(BUILD / "bootloader.bin"), X3_DIR / "bootloader.bin")
    shutil.copyfile(require(BUILD / "partitions.bin"), X3_DIR / "partitions.bin")
    shutil.copyfile(require(BOOT_APP0), X3_DIR / "boot_app0.bin")

    version = read_version()
    manifest = {
        "name": "PocketDeck-OS for Xteink X3",
        "version": version,
        "built": datetime.date.today().isoformat(),
        "new_install_prompt_erase": False,
        "new_install_improv_wait_time": 0,
        "builds": [
            {
                "chipFamily": "ESP32-C3",
                "parts": [
                    {"path": "bootloader.bin", "offset": 0x0},
                    {"path": "partitions.bin", "offset": 0x8000},
                    {"path": "boot_app0.bin", "offset": 0xE000},
                    {"path": APP_NAME, "offset": 0x10000},
                ],
            }
        ],
    }
    (X3_DIR / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    size = DIST_APP.stat().st_size
    print(f"PocketDeck-OS {version}: app {size:,} bytes -> dist/{APP_NAME}")
    print("Updated installer/firmware/x3 (bootloader, partitions, boot_app0, manifest.json)")


def stage(out: Path) -> None:
    require(X3_DIR / "manifest.json")
    require(DIST_APP)
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(INSTALLER, out, ignore=shutil.ignore_patterns(".DS_Store"))
    shutil.copyfile(DIST_APP, out / "firmware" / "x3" / APP_NAME)
    # GitHub Pages: serve files as-is (no Jekyll processing).
    (out / ".nojekyll").write_text("")
    print(f"Staged installer site in {out}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--stage", type=Path, metavar="DIR", help="assemble the static site in DIR")
    args = parser.parse_args()
    if args.stage:
        stage(args.stage.resolve())
    else:
        prepare()


if __name__ == "__main__":
    main()

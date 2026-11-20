#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimusb contributors
"""Lay out one release package and archive it.

    package.py --system win|linux|macos --version 0.4.1 --program PATH \
               --installer PATH --deps BUILD/_deps --out out

The package is one folder, named after the release:

    moonlight-web-mimusb-<version>-<system>/
        installer[.exe]          the window (and command line) that installs bin/
        bin/                     what replaces the official build, with manifest.txt
        mimusb.example.json
        README.txt
        licenses/                this add-on's and the installer's

--program is what goes into bin/: win, the folder of the built program; linux, the
AppImage (installed as MoonlightWeb.AppImage); macos, MoonlightWeb.app.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SYSTEMS = {
    "win": ("win-x64", "installer.exe", "README-windows.txt"),
    "linux": ("linux-x86_64", "installer", "README-linux.txt"),
    "macos": ("macos-arm64", "installer", "README-macos.txt"),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--system", required=True, choices=SYSTEMS)
    parser.add_argument("--version", required=True)
    parser.add_argument("--program", required=True, type=Path)
    parser.add_argument("--installer", required=True, type=Path)
    parser.add_argument("--deps", required=True, type=Path, help="the installer build's _deps folder")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    suffix, installer_name, readme = SYSTEMS[args.system]
    name = f"moonlight-web-mimusb-{args.version}-{suffix}"
    stage = args.out / "stage" / name
    if stage.exists():
        shutil.rmtree(stage)
    (stage / "bin").mkdir(parents=True)

    if args.system == "win":
        shutil.copytree(args.program, stage / "bin", symlinks=True, dirs_exist_ok=True)
    elif args.system == "linux":
        shutil.copy2(args.program, stage / "bin" / "MoonlightWeb.AppImage")
        (stage / "bin" / "MoonlightWeb.AppImage").chmod(0o755)
    else:
        shutil.copytree(args.program, stage / "bin" / "MoonlightWeb.app", symlinks=True)

    subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "make_manifest.py"), str(stage / "bin"),
                    f"v{args.version}"], check=True)

    installer = stage / installer_name
    shutil.copy2(args.installer, installer)
    installer.chmod(0o755)
    shutil.copy2(ROOT / "mimusb.example.json", stage)
    shutil.copy2(ROOT / "tools" / "package" / readme, stage / "README.txt")

    licenses = stage / "licenses"
    licenses.mkdir()
    for source, target in (
        (ROOT / "LICENSE", "moonlight-web-mimusb-LICENSE.txt"),
        (ROOT / "NOTICE", "moonlight-web-mimusb-NOTICE.txt"),
        (args.deps / "imgui-src" / "LICENSE.txt", "imgui-LICENSE.txt"),
        (args.deps / "glfw-src" / "LICENSE.md", "glfw-LICENSE.md"),
        (ROOT / "installer" / "licenses" / "Roboto-LICENSE.txt", "Roboto-LICENSE.txt"),
    ):
        shutil.copy2(source, licenses / target)

    archive = args.out / (name + (".tar.gz" if args.system == "linux" else ".zip"))
    if archive.exists():
        archive.unlink()
    if args.system == "linux":
        with tarfile.open(archive, "w:gz") as tar:
            tar.add(stage, arcname=name)
    elif args.system == "macos":
        # ditto keeps the bundle's links, modes and extended attributes.
        subprocess.run(["ditto", "-c", "-k", "--keepParent", str(stage), str(archive)], check=True)
    else:
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zip_file:
            for path in sorted(stage.rglob("*")):
                zip_file.write(path, Path(name) / path.relative_to(stage))
    print(archive)
    return 0


if __name__ == "__main__":
    sys.exit(main())

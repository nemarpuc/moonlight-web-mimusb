#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimusb contributors
"""Write <bin>/manifest.txt for the installer: the release version, then the
SHA-256 and relative path of every regular file below <bin>.

    make_manifest.py <bin dir> <version>

Symbolic links are skipped (a macOS bundle's links are copied with the bundle);
the installer checks only the files named here.
"""
import hashlib
import os
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    root, version = sys.argv[1], sys.argv[2]
    lines = [f"version\t{version}"]
    for folder, dirs, names in os.walk(root):
        dirs.sort()
        for name in sorted(names):
            path = os.path.join(folder, name)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            if rel == "manifest.txt" or os.path.islink(path):
                continue
            if "\t" in rel or "\n" in rel:
                print(f"error: unsupported file name {rel!r}", file=sys.stderr)
                return 1
            digest = hashlib.sha256()
            with open(path, "rb") as handle:
                for chunk in iter(lambda: handle.read(1 << 20), b""):
                    digest.update(chunk)
            lines.append(f"file\t{digest.hexdigest()}\t{rel}")
    if len(lines) == 1:
        print(f"error: no files below {root}", file=sys.stderr)
        return 1
    with open(os.path.join(root, "manifest.txt"), "w", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

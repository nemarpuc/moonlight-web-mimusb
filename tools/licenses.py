#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimhid contributors
"""Put the license texts and the source pointers into a release payload.

    python tools/licenses.py work <dest-dir>

The binaries are GPL-3.0 (upstream's code plus this add-on) with libmimhid (MIT)
linked in. GPL asks that the license text and a way to get the matching source
travel with them; this writes both into <dest-dir>, from the tree prepare.py made.
"""
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    work, dest = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    info = json.load(open(os.path.join(work, "BUILD_INFO.json"), encoding="utf-8"))
    os.makedirs(dest, exist_ok=True)

    copies = [
        (os.path.join(REPO, "LICENSE"), "LICENSE.txt"),
        (os.path.join(REPO, "NOTICE"), "NOTICE.txt"),
        (os.path.join(info["libmimhid"], "LICENSE"), "libmimhid-LICENSE.txt"),
        (os.path.join(info["source"], "LICENSE"), "moonlight-web-LICENSE.txt"),
        (os.path.join(info["source"], "COPYRIGHT"), "moonlight-web-COPYRIGHT.txt"),
    ]
    for src, name in copies:
        if not os.path.isfile(src):
            sys.exit(f"missing {src}")
        shutil.copyfile(src, os.path.join(dest, name))

    repo = os.environ.get("GITHUB_REPOSITORY", "")
    sha = os.environ.get("GITHUB_SHA", "")
    lines = [
        "This program is free software (GPL-3.0-or-later; see LICENSE.txt).",
        "The corresponding source is:",
        "",
        f"  Moonlight-Web {info['upstream_tag']} (the add-on's patches/ apply to this tag):",
        f"    https://github.com/linckosz/moonlight-web/tree/{info['upstream_tag']}",
        "  This add-on (new files overlay/ and the patches/ applied to it):",
        f"    https://github.com/{repo}" + (f"/tree/{sha}" if sha else "") if repo
        else "    (a local build: see the repository this was built from)",
        f"  libmimhid (MIT) {info['libmimhid_tag']}:",
        f"    https://github.com/nemarpuc/libmimhid/tree/{info['libmimhid_tag']}",
        "",
        "To rebuild: python tools/prepare.py --tag " + info["upstream_tag"]
        + " --libmimhid-tag " + info["libmimhid_tag"] + " --out work",
        "then the platform build described in the add-on's README.",
        "",
    ]
    with open(os.path.join(dest, "SOURCE.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("licenses ->", dest)


if __name__ == "__main__":
    main()

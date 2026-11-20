#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimusb contributors
"""Gate before a release: is this build one the official entry page will open?

    python tools/verify.py work [--wait-minutes 60]

The entry page (stream.moonlightweb.top) refuses a machine whose interface is
not byte-for-byte the release's, and a build whose version is not a release.
This checks, for the tree prepare.py made:

  1. the frontend and bootstrap directories are untouched (git sees no change);
  2. upstream's own check passes for that release (its committed list matches
     the tag);
  3. the official list for that version is published at the entry server
     (right after a release it may not be yet: --wait-minutes polls for it).

Exit status 0 only when all three hold; 3 when only the third fails (try later).
"""
import argparse
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request

ENTRY = "https://stream.moonlightweb.top"


def list_published(version):
    url = f"{ENTRY}/v1/interfaces/{version}.json"
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "moonlight-web-mimusb"}), timeout=30) as r:
            body = json.load(r)
        return body.get("version") == version and bool(body.get("files"))
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return False
        raise


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("work")
    ap.add_argument("--wait-minutes", type=int, default=0)
    args = ap.parse_args()

    work = os.path.abspath(args.work)
    info = json.load(open(os.path.join(work, "BUILD_INFO.json"), encoding="utf-8"))
    src, version = info["source"], info["version"]

    # 1. untouched
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "frontend", "bootstrap"], cwd=src, text=True).strip()
    if dirty:
        sys.exit("frontend/bootstrap differ from the release:\n" + dirty)
    print("ok: frontend and bootstrap are the release's")

    # 2. upstream's own gate
    # It also holds older releases' lists against their tags, which a shallow
    # clone does not have; only this release's own verdict matters here.
    result = subprocess.run(
        ["node", "scripts/interface-list.mjs", "check", "--release", version],
        cwd=src, text=True, capture_output=True)
    print(result.stdout, end="")
    verdict = [l for l in result.stdout.splitlines() if l.split() and l.split()[0] in ("ok", "FAIL", "fail")
               and f" {version} " in l + " "]
    if not any(l.startswith("ok") for l in verdict):
        sys.exit(f"upstream's check does not pass for {version}:\n{result.stdout}{result.stderr}")

    # 3. published
    deadline = time.time() + args.wait_minutes * 60
    while not list_published(version):
        if time.time() >= deadline:
            print(f"the official list for {version} is not published at {ENTRY} (yet)")
            sys.exit(3)
        print("waiting for the official list of", version, flush=True)
        time.sleep(300)
    print(f"ok: the official list for {version} is published")


if __name__ == "__main__":
    main()

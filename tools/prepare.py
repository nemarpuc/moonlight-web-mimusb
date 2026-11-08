#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimhid contributors
"""Prepare a build tree: upstream at a tag, plus this add-on.

Used by the workflow and by hand, so both take the same road:

    python tools/prepare.py --tag v0.4.1 --out work

1. clone upstream at the tag, with submodules;
2. copy overlay/ over it;
3. append one include line to backend/native-host/CMakeLists.txt (the CMake
   side of the add-on lives in overlay/.../cmake/MimHid.cmake, so upstream's file
   needs no patch hunk that could stop applying);
4. point the update check at THIS repository's releases (one string in
   backend/src/network/UpdateChecker.cpp), so a user is told about a new version
   only when this add-on has built it; see redirect_update_source;
5. apply patches/ in order (a patch that does not apply stops everything and
   says where each hunk's first line moved, or that it is gone);
6. clone libmimhid at its newest release tag (or the one given with --libmimhid-tag);
7. write work/BUILD_INFO.json with what was used.

`--tag latest` asks GitHub for upstream's newest v* release tag. `--ref <branch>`
builds a tree from a branch instead (a look-ahead check; never released).
Only the standard library is used.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request

UPSTREAM = "https://github.com/linckosz/moonlight-web.git"
UPSTREAM_API = "https://api.github.com/repos/linckosz/moonlight-web/tags?per_page=100"
LIBMIMHID = "https://github.com/nemarpuc/libmimhid.git"
LIBMIMHID_API = "https://api.github.com/repos/nemarpuc/libmimhid/releases/latest"

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)


def run(cmd, cwd=None):
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=cwd)


def release_tag_key(tag):
    m = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)", tag)
    return tuple(int(x) for x in m.groups()) if m else None


def latest_upstream_tag():
    """Newest vX.Y.Z tag (the 'srv-' tags are the website's, not releases)."""
    req = urllib.request.Request(UPSTREAM_API, headers={"User-Agent": "moonlight-web-mimhid"})
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=60) as r:
        tags = [t["name"] for t in json.load(r)]
    keyed = [(release_tag_key(t), t) for t in tags if release_tag_key(t)]
    if not keyed:
        sys.exit("no release tag found upstream")
    return max(keyed)[1]


def latest_libmimhid_tag():
    """Tag of libmimhid's newest published release (the one that carries firmware images).

    Nothing is guessed: if GitHub cannot say, the build stops.
    """
    req = urllib.request.Request(LIBMIMHID_API, headers={"User-Agent": "moonlight-web-mimhid"})
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=60) as r:
        tag = json.load(r).get("tag_name", "")
    if not release_tag_key(tag):
        sys.exit(f"libmimhid's newest release has the tag {tag!r}, which is not vX.Y.Z")
    return tag


CMAKE_FILE = os.path.join("backend", "native-host", "CMakeLists.txt")
CMAKE_INCLUDE = 'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MimHid.cmake")'


def append_cmake_include(src):
    path = os.path.join(src, CMAKE_FILE)
    with open(path, newline="", encoding="utf-8") as f:
        text = f.read()
    if "add_library(mw-native-host" not in text:
        sys.exit(f"{CMAKE_FILE}: no 'add_library(mw-native-host' any more; MimHid.cmake needs that target")
    nl = "\r\n" if "\r\n" in text else "\n"
    with open(path, "w", newline="", encoding="utf-8") as f:
        f.write(text.rstrip("\r\n") + nl + nl + "# moonlight-web-mimhid add-on" + nl + CMAKE_INCLUDE + nl)
    print("appended to", CMAKE_FILE.replace(os.sep, "/"))


UPDATE_FILE = os.path.join("backend", "src", "network", "UpdateChecker.cpp")
UPSTREAM_RELEASES = "https://api.github.com/repos/linckosz/moonlight-web/releases/latest"


def redirect_update_source(src, repo):
    """Make the built-in update check read this repository's latest release.

    Upstream asks its own repo, so a user would be offered (and could install) an
    official version before this add-on has built it, and the official build has
    no board support. The release here has the same tag as upstream's, so the
    version comparison is unchanged. An exact string match: if upstream moves the
    URL the build stops here instead of silently checking the wrong place.
    """
    if not repo:
        print("update check: left at upstream (no --addon-repo, no GITHUB_REPOSITORY): a local build")
        return
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
        sys.exit(f"{repo!r} is not owner/name")
    path = os.path.join(src, UPDATE_FILE)
    with open(path, newline="", encoding="utf-8") as f:
        text = f.read()
    if text.count(UPSTREAM_RELEASES) != 1:
        sys.exit(f"{UPDATE_FILE}: expected the upstream releases URL exactly once; "
                 "the update check moved, review redirect_update_source")
    with open(path, "w", newline="", encoding="utf-8") as f:
        f.write(text.replace(UPSTREAM_RELEASES, f"https://api.github.com/repos/{repo}/releases/latest"))
    print(f"update check now reads github.com/{repo} releases")


def explain_failed_patch(src, patch):
    """Where did each hunk's first line go? Printed when a patch does not apply."""
    print(f"\n--- {os.path.basename(patch)} does not apply; upstream moved. Hunks:", flush=True)
    target = None
    first = None
    for line in open(patch, errors="replace", encoding="utf-8").read().splitlines() + ["@@end"]:
        if line.startswith("--- "):
            continue
        if line.startswith("+++ b/"):
            target = os.path.join(src, line[6:])
        elif line.startswith("@@"):
            if first is not None and target:
                report_hunk(target, first)
            first = None
        elif first is None and line[:1] in (" ", "-") and line[1:].strip():
            first = line[1:]
    print("--- fix the patch against the new upstream, then re-run.", flush=True)


def report_hunk(target, first):
    try:
        lines = open(target, errors="replace", encoding="utf-8").read().splitlines()
    except OSError:
        print(f"  {os.path.relpath(target)}: file is gone")
        return
    hits = [i + 1 for i, l in enumerate(lines) if l.strip() == first.strip()]
    where = ("line " + ", ".join(map(str, hits[:5]))) if hits else "NOT FOUND (changed or removed)"
    print(f"  {os.path.basename(target)}: {first.strip()[:70]!r} -> {where}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", default="latest", help="upstream tag, e.g. v0.4.1, or 'latest'")
    ap.add_argument("--out", default="work", help="directory to build in (must not exist)")
    ap.add_argument("--ref", help="build from this upstream branch instead of a release tag (look-ahead check)")
    ap.add_argument("--addon-repo", default=os.environ.get("GITHUB_REPOSITORY", ""),
                    help="owner/name whose releases the built-in update check reads "
                         "(default: $GITHUB_REPOSITORY; empty keeps upstream's)")
    ap.add_argument("--libmimhid-tag", default="latest",
                    help="libmimhid release tag, e.g. v0.2.0, or 'latest' (its newest release)")
    ap.add_argument("--print-latest", action="store_true", help="print upstream's newest tag and exit")
    ap.add_argument("--print-libmimhid-latest", action="store_true",
                    help="print libmimhid's newest release tag and exit")
    args = ap.parse_args()

    if args.print_latest:
        print(latest_upstream_tag())
        return
    if args.print_libmimhid_latest:
        print(latest_libmimhid_tag())
        return

    tag = args.ref or (latest_upstream_tag() if args.tag == "latest" else args.tag)
    if not args.ref and not release_tag_key(tag):
        sys.exit(f"{tag!r} is not a release tag (vX.Y.Z)")
    out = os.path.abspath(args.out)
    if os.path.exists(out):
        sys.exit(f"{out} already exists; remove it or choose another --out")
    os.makedirs(out)
    src = os.path.join(out, "moonlight-web")

    # 1. upstream. core.autocrlf=false: the frontend must stay byte-identical
    #    to the release (the entry page holds it to the published digests).
    run(["git", "-c", "core.autocrlf=false", "clone", "--branch", tag, "--depth", "1",
         "--recurse-submodules", "--shallow-submodules", UPSTREAM, src])
    run(["git", "config", "core.autocrlf", "false"], cwd=src)

    # 2. overlay
    overlay = os.path.join(REPO, "overlay")
    for root, _dirs, files in os.walk(overlay):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, overlay)
            dest = os.path.join(src, rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copyfile(full, dest)
            print("overlay:", rel.replace(os.sep, "/"))

    # 3. the one CMake line
    append_cmake_include(src)

    # 4. the update source
    redirect_update_source(src, args.addon_repo)

    # 5. patches. --3way would paper over a real change; a strict apply is what
    #    tells the owner that upstream moved.
    patches = sorted(p for p in os.listdir(os.path.join(REPO, "patches")) if p.endswith(".patch"))
    for p in patches:
        full = os.path.join(REPO, "patches", p)
        try:
            run(["git", "apply", "--whitespace=nowarn", full], cwd=src)
        except subprocess.CalledProcessError:
            explain_failed_patch(src, full)
            sys.exit(1)

    # 6. libmimhid: its newest release unless a tag was named. The host library and the
    #    board's firmware must be the same version, so the tag is recorded and shown.
    libmimhid_tag = latest_libmimhid_tag() if args.libmimhid_tag == "latest" else args.libmimhid_tag
    if not release_tag_key(libmimhid_tag):
        sys.exit(f"{libmimhid_tag!r} is not a libmimhid release tag (vX.Y.Z)")
    lib = os.path.join(out, "libmimhid")
    run(["git", "clone", "--branch", libmimhid_tag, "--depth", "1", LIBMIMHID, lib])

    info = {"upstream_tag": tag, "version": tag.lstrip("v"), "libmimhid_tag": libmimhid_tag,
            "source": src, "libmimhid": lib}
    with open(os.path.join(out, "BUILD_INFO.json"), "w", encoding="utf-8") as f:
        json.dump(info, f, indent=2)
    print(json.dumps(info, indent=2))


if __name__ == "__main__":
    main()

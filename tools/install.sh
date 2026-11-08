#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimhid contributors
# Install or remove the add-on build on Linux and macOS (Windows: install.ps1).
#
#   install.sh install <AppImage | MoonlightWeb zip> [--prefix DIR]
#   install.sh update [--prefix DIR]     fetch this add-on's newest release and install it
#   install.sh rollback [--prefix DIR]
#
# Linux : copies the AppImage to <prefix>/MoonlightWeb.AppImage (default prefix
#         ~/.local/bin) and adds a menu entry. An existing file is kept once as
#         *.official. Needs the serial port's group (dialout / uucp).
# macOS : puts MoonlightWeb.app into <prefix> (default /Applications). An existing
#         MoonlightWeb.app is kept once as MoonlightWeb.app.official.
# rollback puts the *.official copy back (or removes the add-on if there was none).
# mimhid.json is never touched; see README for where it lives.
set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

cmd="${1:-}"; [ $# -gt 0 ] && shift
src=""; prefix=""
while [ $# -gt 0 ]; do
  case "$1" in
    --prefix) prefix="${2:?--prefix needs a directory}"; shift 2 ;;
    -*) die "unknown option $1" ;;
    *) src="$1"; shift ;;
  esac
done

os="$(uname -s)"
case "$os" in
  Linux)  prefix="${prefix:-$HOME/.local/bin}"; target="$prefix/MoonlightWeb.AppImage" ;;
  Darwin) prefix="${prefix:-/Applications}";    target="$prefix/MoonlightWeb.app" ;;
  *) die "unsupported OS $os" ;;
esac
desktop_file="${XDG_DATA_HOME:-$HOME/.local/share}/applications/moonlight-web-mimhid.desktop"

# SHA-256 of the one file that identifies an installed build.
fingerprint() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi
}
# Is $1 (with fingerprint file $2, marker $3) exactly what this script installed last time?
is_ours() { [ -f "$3" ] && [ "$(fingerprint "$2")" = "$(cat "$3")" ]; }

# Move the genuine file aside as <path>.official, replacing an older copy: after an
# update by upstream the file in place is the newest official one, and rollback
# must never bring back an older version.
keep_official() { rm -rf "$1.official"; mv "$1" "$1.official"; }

stop_running() {
  pkill -x MoonlightWeb 2>/dev/null || true
  # Anchored: an unanchored -f pattern would also match this shell's own command line.
  [ "$os" = Linux ] && { pkill -f "^$target" 2>/dev/null || true; }
  sleep 1
}

do_install() {
  if [ -z "$src" ] || [ ! -f "$src" ]; then die "give the AppImage (Linux) or the zip (macOS) to install"; fi
  mkdir -p "$prefix"
  stop_running
  if [ "$os" = Linux ]; then
    # Only a file that is not what this script installed is kept as official.
    if [ -e "$target" ] && ! is_ours "$target" "$target" "$target.mimhid"; then keep_official "$target"; fi
    install -m 0755 "$src" "$target"
    fingerprint "$target" > "$target.mimhid"
    mkdir -p "$(dirname "$desktop_file")"
    printf '[Desktop Entry]\nType=Application\nName=MoonlightWeb (mimhid)\nExec="%s"\nTerminal=false\nCategories=Network;\n' \
      "$target" > "$desktop_file"
    id -nG | tr ' ' '\n' | grep -qxE 'dialout|uucp' ||
      echo "note: add yourself to the serial group for the board: sudo usermod -aG dialout \$USER (uucp on Arch), then sign in again"
  else
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp" "${dl:-}"' EXIT
    ditto -x -k "$src" "$tmp"
    [ -d "$tmp/MoonlightWeb.app" ] || die "no MoonlightWeb.app in $src"
    exe="Contents/MacOS/MoonlightWeb"
    if [ -e "$target" ] && ! is_ours "$target" "$target/$exe" "$target/Contents/mimhid.marker"; then keep_official "$target"; else rm -rf "$target"; fi
    ditto "$tmp/MoonlightWeb.app" "$target"
    fingerprint "$target/$exe" > "$target/Contents/mimhid.marker"
    xattr -dr com.apple.quarantine "$target" 2>/dev/null || true
  fi
  echo "Installed: $target"
  echo "Settings: mimhid.json beside settings.json (see README). Undo: install.sh rollback"
}

# The repository is filled in by the release workflow; a copy from a checkout has none.
REPO="@REPO@"

do_update() {
  case "$REPO" in @*) die "this copy does not know its repository; download install.sh from a release" ;; esac
  command -v curl >/dev/null 2>&1 || die "curl is needed for update"
  local pattern
  if [ "$os" = Linux ]; then pattern='x86_64\.AppImage'; else pattern='macos-arm64\.zip'; fi
  local json url name tmp
  json="$(curl -fsSL -H 'Accept: application/vnd.github+json' "https://api.github.com/repos/$REPO/releases/latest")" ||
    die "could not read the newest release of $REPO"
  url="$(printf '%s\n' "$json" | grep -o '"browser_download_url": *"[^"]*"' | sed 's/.*: *"//; s/"$//' | grep -E "$pattern\$" | head -n1)"
  [ -n "$url" ] || die "the newest release has no file for this system yet"
  name="${url##*/}"
  tmp="$(mktemp -d)"; dl="$tmp"; trap 'rm -rf "$tmp"' EXIT
  echo "Downloading $name ..."
  curl -fsSL -o "$tmp/$name" "$url" || die "download failed"
  src="$tmp/$name"
  do_install
}

do_rollback() {
  stop_running
  if [ -e "$target.official" ]; then
    rm -rf "$target"; mv "$target.official" "$target"; echo "Restored the official build."
  elif [ -e "$target" ]; then
    rm -rf "$target"; echo "Removed the add-on (no official copy was kept)."
  else
    echo "Nothing to roll back at $target."
  fi
  rm -f "$target.mimhid" "$desktop_file"
}

case "$cmd" in
  install)  do_install ;;
  update)   do_update ;;
  rollback) do_rollback ;;
  *) sed -n '4,7p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac

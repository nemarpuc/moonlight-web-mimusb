# moonlight-web-mimhid

**Unofficial** add-on for [Moonlight-Web](https://github.com/linckosz/moonlight-web).
It sends the viewer's **keyboard and mouse** through a
[libmimhid](https://github.com/nemarpuc/libmimhid) board — a Raspberry Pi Pico
(or other supported chip) on a serial port that appears to a computer as a
**real USB keyboard and mouse**. The board's USB side can be plugged into the
very PC that runs the server, so a remote session exercises the same path a
physical keyboard takes.

> Not affiliated with, or endorsed by, the Moonlight-Web author. This repository
> holds no upstream source: only new files, four small patches and one appended
> CMake line, applied to the official release by the build. See [DESIGN.md](DESIGN.md).

## What you need

- A board running libmimhid's firmware (Pico / Pico 2 are the supported ones)
  and a 3.3 V USB-to-UART bridge, wired as in libmimhid's
  [QUICKSTART](https://github.com/nemarpuc/libmimhid/blob/main/docs/QUICKSTART.md).
  Two USB ports are used when the board is plugged into the same PC.
- A host PC: Windows 10/11 x64, Linux x86_64, or macOS (Apple silicon). Windows is the only one
  tried on hardware.

## Install

Take the newest release of this repository. It holds one package per system. Unpack it and run
**`installer`**: a small window that puts the build in `bin/` in place of the official
MoonlightWeb, and lets you fill in the board's settings (port, USB VID/PID, names) and save
`mimhid.json`. The package also carries `mimhid.example.json`, a README and the licenses.

| System | Package | Run |
|---|---|---|
| Windows 10/11 x64 | `moonlight-web-mimhid-<ver>-win-x64.zip` | `installer.exe` (asks for administrator rights) |
| Linux x86_64 | `moonlight-web-mimhid-<ver>-linux-x86_64.tar.gz` | `./installer` |
| macOS, Apple silicon | `moonlight-web-mimhid-<ver>-macos-arm64.zip` | `installer` (ad hoc signed: allow it in System Settings > Privacy & Security) |

Press **Install**; **Back to official** undoes it. Without a screen: `installer status`,
`installer install`, `installer rollback`.

Windows replaces the files of an installed official MoonlightWeb (install that first). Linux puts
the AppImage in `~/.local/bin` with a menu entry; macOS puts `MoonlightWeb.app` in `/Applications`.
Every file or app it replaces is kept once as `*.official`, and the undo step puts it back. The
installer checks every file of `bin/` against `bin/manifest.txt` before it copies anything, and
again after.

Upstream's autostart and stream-worker tasks keep working, as they already point
at the install folder.

**Updates.** The built-in update notice reads *this* repository's releases, so you are told about
a new version only once it exists here, as the add-on build, and never offered the official
installer. The notice links the release page ("What's new") and says to update the host PC; there is
no one-click update. Download the new release and run its `installer` the same way; your
`mimhid.json` stays. If you run upstream's own installer by hand, you have the official build
again: run the add-on's installer again.

The release is built from the official tag with the official frontend, so the
official entry link (`stream.moonlightweb.top/…`) opens it as before.

## Releases and versions

- **The tag is upstream's.** Moonlight-Web `v0.4.1` becomes the release `v0.4.1` here, so the
  version a machine reports is a real release number. The official entry page
  (`stream.moonlightweb.top`) opens only official versions, file for file.
- **What a release holds:** exactly three packages (Windows x64, Linux x86_64, macOS Apple silicon),
  each with the installer, the `bin/` it installs, `mimhid.example.json` and a README. Every download carries the license
  texts and the way to get the exact source. The systems are the ones upstream builds: Windows x64
  and ARM64, Linux x64, macOS arm64 (see [DESIGN.md](DESIGN.md#what-is-built-and-what-is-not)).
- **Built automatically** once a day. A new upstream release is built and published here, normally
  within a day of it. Until then, a build you already run keeps working: the entry page judges a
  machine against the list of the version it reports, not against the newest one.
- **libmimhid is always its newest release.** The release notes name the version
  (`libmimhid: v0.2.0`). The board's firmware must be the same version: write that version's
  firmware (RP2040: `mimhid_firmware-rp2040-*.uf2`, RP2350: `mimhid_firmware-rp2350-*.uf2`, other chips
  on the same page) from the [libmimhid release](https://github.com/nemarpuc/libmimhid/releases).
  When a build uses a newer libmimhid than the release it replaces, the notes say so at the top.
  Such a rebuild keeps the same release tag, so the in-app update notice does not announce it:
  watch this repository's releases.
- **If a patch stops applying** after an upstream change, nothing is published until it is fixed;
  what you run keeps working.
- Details, with sources: [DESIGN.md](DESIGN.md#versions-the-entry-page-and-updates).

## Configure

The installer's window edits the board's keys and saves `mimhid.json` beside upstream's
`settings.json`; the rest of the file stays as it is. You can also copy
[`mimhid.example.json`](mimhid.example.json) there and edit it by hand. Restart MoonlightWeb after a change.
Moonlight-Web itself has no settings page for this.

| OS | Folder |
|---|---|
| Windows | `%APPDATA%\MoonlightWeb\MoonlightWeb` |
| Linux | `~/.local/share/MoonlightWeb/MoonlightWeb` |
| macOS | `~/Library/Application Support/MoonlightWeb/MoonlightWeb` |

`MW_MIMHID_CONFIG=<path>` points elsewhere. To use the OS input again without
uninstalling, set `"enabled": false` and restart. With no board connected, input
goes through the OS on its own, and moves to the board within 5 seconds of it
answering.

**Nothing has a default.** With `"enabled": true`, every key of the example must be
present (libmimhid fills nothing in, and neither does this add-on); a missing or
out-of-range key stops the add-on with its full name in the log (for example
`missing key devices[0].usb.language_id`). The example holds a working set for a
full-speed board: the `link` timeouts (`send_timeout_ms` bounds one report's write; it never waits for the target) and report size, the `usb` descriptor values, and per
class the HID version, endpoint address and packet size, usage or axis ranges. A zero `baud`
is allowed only with `"port": "auto"` (the fastest rate discovery found).

**One board, one device:** put the device's keys at the top level (as the
example's single entry, without the `devices` list). **Two boards**, e.g. the
keyboard on one and the mouse on the other: a `devices` list with one complete entry
per board, each carrying one class (see the example for the full keys).

A class is presented only if its object is there. `"port": "auto"` takes the next board
that no other entry names. The IDs in the example are pid.codes' test pair for your own
testing; a device you hand to others needs IDs of its own — never copy another maker's.

## Troubleshooting

- **Nothing reaches the target.** The log says whether the board opened; check
  `port` (`auto` finds the first board) and that the serial port is yours:
  Linux needs the `dialout` (Debian, Ubuntu) or `uucp` (Arch) group, then sign in again.
- **The first 5 seconds after plugging the board** still use the OS input; this is by design.
- **After upstream's own installer** the official build is back: run the add-on's `installer` again.
- **The entry page refuses the build.** The build's version must be an official
  release; a build from a branch is never published (see [DESIGN.md](DESIGN.md)).

## Security

A board that types for you is a real keyboard to the target: whoever can reach a
Moonlight-Web session as its owner can type on that machine. Only the owner is
routed to the board (guests stay on the OS path with upstream's gate). Keep the
board's serial port and `mimhid.json` to your own account.

## Limits

See [DESIGN.md](DESIGN.md#known-limits). In short: relative mouse only (absolute
positions become moves), US-layout text typing, guests stay on the OS input path,
and only the Windows build is exercised on hardware.

## Building

`python tools/prepare.py --tag latest --out work`, then
`tools/verify.py work`, then `tools/build-windows.ps1 -Work work -QtDir <Qt>`.
The installer is built on its own: `cmake -S installer -B build-installer` (see
`installer/`); `tools/package.py` lays out the release package.
The GitHub workflow does the same daily for new upstream releases, and
`patches.yml` checks on every push (and against upstream's `main`) that the
patches still apply. Linux and macOS are built by the same workflow.

Tests (from this directory, any C++17 compiler):

```sh
c++ -std=c++17 -Ioverlay/backend/native-host/src/input tests/test_config.cpp \
    overlay/backend/native-host/src/input/MimHidConfig.cpp -o /tmp/tc && /tmp/tc
```

## License and source

**GPL-3.0-or-later**, as upstream: the add-on's code implements upstream's
interface and its patches contain upstream lines, so what you download is one GPL
work (see [LICENSE](LICENSE) and [NOTICE](NOTICE)). [libmimhid](https://github.com/nemarpuc/libmimhid)
is MIT and linked in; its text is shipped too.

Every download carries its licenses and the way to get the exact source
(`licenses/` inside the zip and the `.app`, `usr/share/doc/moonlight-web-mimhid/`
inside the AppImage):

- Moonlight-Web at the release's tag — <https://github.com/linckosz/moonlight-web>
- this repository at the commit named in the release notes (`overlay/`, `patches/`)
- libmimhid at the tag named in the release notes (`libmimhid: vX.Y.Z`)

To rebuild exactly what a release holds, run `tools/prepare.py` with its tag (see Building).

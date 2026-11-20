# Design

An **unofficial** add-on for [Moonlight-Web](https://github.com/linckosz/moonlight-web)
(GPL-3.0, © Bruno Martin). It sends the viewer's keyboard and mouse through
[libmimusb](https://github.com/nemarpuc/libmimusb) (MIT): a microcontroller
board on a serial port that appears to a machine as a **real USB keyboard and
mouse**. The board's USB side may be plugged into the very PC that runs the
server.

## Goals and non-goals

| Goal | Non-goal |
|---|---|
| The smallest possible change to upstream | A settings page or API (they break with every upstream refactor) |
| Works through the official `stream.moonlightweb.top` entry link | Gamepads, touch, pen, raw HID (add when asked for) |
| Windows, Linux, macOS hosts | A fork: no upstream source is stored here |
| One board, or several (keyboard and mouse on different boards) | Pretending to be another maker's USB device |

## How it fits

```
browser ─ WebRTC ─▶ InputEvent ─▶ IInputSink::inject()
                                      │
                         ┌────────────┴─────────────┐
                         ▼                          ▼
                MimUsbInput (this repo)      the OS sink upstream built
                 keyboard/mouse → board       (SendInput / uinput / CGEvent)
                 everything else ───────────▶  gamepads, Ctrl+Alt+Del, ...
```

`MimUsbInput` **decorates** the sink upstream already creates. What it does not
route goes to that sink unchanged, so an unconfigured or unplugged board costs
a log line, not the session.

### Routing

- Keyboard events go to the first configured device with `keyboard.enabled`.
- Mouse events go to the first configured device with `mouse.enabled`.
- A class no device carries stays on the OS path.
- **Guests are never routed to a board.** Upstream's gate that keeps invited
  players out of administrator windows lives in the OS sink; a physical device
  would walk around it. Only the owner (`allowElevatedInput`) is routed.

### One device, or several

`devices` is a list; each entry is one board on one serial port.

- A device with both `keyboard` and `mouse` is one composite USB device: one
  **interface** per class (not Report IDs), because the boot protocol (BIOS,
  bootloaders) needs a keyboard and a mouse on interfaces of their own.
- Two boards, one class each, are two independent USB devices.
- `"port": "auto"` takes the next board discovery finds that no other entry has
  claimed, so two `auto` entries get two different boards.
- A board that cannot be opened drops only its own classes back to the OS.

### Process-wide boards

Opening a board makes the target see a USB device plugged in. Boards are
opened once per process and kept, so a keyboard does not vanish and reappear at
every stream. Changing `mimusb.json` takes a restart.

## Configuration

`mimusb.json`, beside upstream's `settings.json`
(`%APPDATA%\MoonlightWeb\MoonlightWeb`, `~/.local/share/MoonlightWeb/MoonlightWeb`,
`~/Library/Application Support/MoonlightWeb/MoonlightWeb`), or the path in
`MW_MIMUSB_CONFIG`. `"enabled": false` turns the add-on off. (`MW_MIMUSB_DISABLE=1`
exists too, but a stream worker is started by a scheduled task and never sees it.)
See `mimusb.example.json`. No key has a default: `parseMimUsbConfig` refuses a file that leaves one out and names it, and `MimUsbInput.cpp` passes every value to libmimusb unchanged.

Edited by hand; there is no UI.

## What is patched in upstream

Five existing files, a few lines each. Four are patches (`patches/`, written with one line of
context so a neighbouring upstream change does not stop them applying); the fifth gets one line
appended by `tools/prepare.py`, so it has no hunk that can stop applying:

| File | Change |
|---|---|
| `backend/native-host/CMakeLists.txt` | one appended `include(cmake/MimUsb.cmake)`; the `MW_MIMUSB` option and the build of libmimusb live in `overlay/.../cmake/MimUsb.cmake` |
| `…/platform/windows/WindowsSession.cpp` | wrap the sink: `wrapWithMimUsb(...)` |
| `…/platform/linux/LinuxSession.cpp` | the same |
| `…/platform/macos/MacSession.mm` | the same |
| `backend/src/main.cpp` | `/api/update/check`: `self_update.supported` is false when the update checker found no installer for this platform (see below) |

One more line is rewritten by `prepare.py` in CI: the update check's URL in
`backend/src/network/UpdateChecker.cpp` now names this repository's releases
(`$GITHUB_REPOSITORY`) instead of upstream's. The release here has upstream's tag,
so version comparison is unchanged, but a user is told about a new version only
when this add-on has built it. Windows and macOS assets here are zips, which the
updater's one-click path does not take, so it shows the release page; install with
the package's `installer`. The exact-match check stops the build if upstream
moves that URL. Local builds (no `GITHUB_REPOSITORY`) keep upstream's.

Everything else is new files in `overlay/`, copied in as they are. **The
frontend is never touched**: the official entry page holds every frontend file
against the release's published list and refuses a machine that differs
(`bootstrap/v1/boot.js`), and a build whose version is not an official release
is refused too. So a build must carry the upstream tag's exact version, with
the exact frontend, which `tools/prepare.py` and the workflow guarantee.

## Build and release

`.github/workflows/build.yml`, daily and on demand:

1. Read upstream's newest `v*` tag and libmimusb's newest release; stop if this repo already
   released that tag, complete, built with that libmimusb (the release notes' `libmimusb:` line).
2. `tools/prepare.py`: clone upstream at the tag with submodules, copy
   `overlay/`, apply `patches/`. A patch that does not apply **fails the run**
   (GitHub then mails the owner); nothing is published.
3. Fetch libmimusb at its newest release (all three platforms use the tag decided in step 1),
   build with `MW_MIMUSB=ON` and `MW_VERSION=<tag without v>`.
4. Check the frontend against the official list for that version.
5. Publish the packages with the source links GPL-3.0 asks for. The release carries upstream's tag;
   its notes state the libmimusb version, tell the user to flash that version's firmware, and,
   when libmimusb changed since the release being replaced, say so at the top. It is marked
   "Latest" only when it is upstream's newest tag.

Windows (zip), Linux (tar.gz holding the AppImage) and macOS (arm64 zip) are all packaged; only
Windows has run on hardware.

`.github/workflows/patches.yml` runs on every push and daily: overlay and patches
onto upstream's newest release (must pass) and onto upstream `main` (look-ahead:
a failure there warns that the next release will break). When a patch fails,
`prepare.py` prints where each hunk's first line went in the new upstream.

## Versions, the entry page and updates

What follows was read from upstream's code and from the live entry server on
2026-11-08. Upstream is `linckosz/moonlight-web` at commit `e31fd43` (2026-11-07);
paths below are in that tree. Upstream may change any of it; re-check before relying on it.

### How the entry page judges a version

- A machine reports a version in `/api/app/manifest`. The entry page takes the release
  number out of it (`bootstrap/v1/official.js`, `appVersionOf`) and downloads **the list
  for that number** from `/v1/interfaces/<version>.json` (`fetchList`, same file).
- It then compares every frontend file the machine sends with that list
  (`bootstrap/v1/boot.js`, `officialCheck` and `shellIsTrusted`).
  A file that differs, or a version with no list (HTTP 404), is refused when
  `PRODUCTION_ENFORCES` is true, which it is (`official.js`, since upstream v0.4.1).
  A list that cannot be fetched at all (not a 404) stops the start with "reload the page".
- The check is **per version, not "newest only"**. Nothing in `bootstrap/v1/*.js` refuses a
  version for being old. Observed on `stream.moonlightweb.top`: lists exist for 0.3.0,
  0.3.1, 0.4.0 and 0.4.1 (HTTP 200); 0.4.2 (not released yet) and 9.9.9 give 404.
- Upstream makes the list part of the release: `scripts/interface-list.mjs` builds it,
  the release commit carries it, and CI's tag job checks that production serves it
  (`.github/workflows/ci.yml`, job `interface-lists`, `live`).
  So a list normally exists when upstream's release does.
- Not known: whether upstream will ever remove old lists. Upstream announces an end
  date only for installs of v0.2.4 or earlier (their sub-domains stop in February 2027,
  `README.md`); those versions have no list and are already outside this check.

### What this add-on's build does about it

It is built from the upstream tag with the official frontend, so its version is a real
release number and its files match that release's list. `prepare.py --tag` and
`verify.py` keep it so; `build.yml` skips a tag whose list is not served yet and tries
again the next day.

### What happens today, step by step

When upstream publishes release X:

1. Upstream's list for X is served by the entry server (by their CI rule above).
2. Within about a day (`build.yml`, cron 03:17 UTC) this repository builds X and
   publishes it. A platform that failed is retried daily and added to the same release.
3. Until then, a machine **already running an older add-on build keeps working**: its
   own version's list is still there. Nobody is locked out by X existing.
4. The built-in update notice reads this repository's `releases/latest`, so it says
   nothing about X until step 2 is done, and then offers the add-on build. The notice
   (`/api/update/check`) picks the release asset by name: Windows `win-x64.exe`, macOS `.pkg`,
   Linux `.deb` / `.rpm` / `.AppImage` (`UpdateChecker::pickAsset`, upstream `backend/src/network/`).
   - **Every system:** this repository's assets are archives (`win-x64.zip`,
     `linux-x86_64.tar.gz`, `macos-arm64.zip`) that carry their own installer, so none matches
     upstream's names. Upstream's banner would still offer an Update button, which fails with
     "No installer published for this platform", because `self_update.supported` ignores the
     asset. `patches/0004` makes it false when no asset matched, so the banner reads "update the
     host PC to install it" with a link to the release page; the user installs the new release
     the same way. The elevated Windows helper
     (`--update-apply`) would refuse anyway: it demands `<product>-installer-*win-x64.exe` with a
     SHA-256 (`UpdateApplyRules`).
5. If a user runs upstream's own installer by hand, they get the official build of X: it works
   with the entry page, but input goes through the OS again until the add-on's `installer`
   is run again.
6. If a patch no longer applies, nothing is published (the workflow fails and GitHub
   mails the owner). Machines keep running their current build; no new version for
   anyone until the patch is fixed.

A build that is **not** an upstream tag version (a local build from a branch, or one
whose tag differs) has no list, so the entry page refuses it. Local builds are for
trying on the LAN, not through the entry link.

### GitHub API use

Used by `prepare.py` (tags), `build.yml` (release lookup, `GITHUB_TOKEN`)
and the app's update check (releases/latest). A few calls a day from the workflows. GitHub's
documented limits are far above that, and the add-on's own use is nowhere near them:
unauthenticated 60 requests/hour per IP, authenticated 5000/hour, an Actions
`GITHUB_TOKEN` 1000/hour per repository
(<https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api>).
GitHub also disables scheduled workflows in a public repository after 60 days without
repository activity (<https://docs.github.com/en/actions/managing-workflow-runs-and-deployments/managing-workflow-runs/disabling-and-enabling-a-workflow>):
check that `build.yml` is still enabled if releases stop appearing.

## What is built and what is not

Upstream's release workflow (`.github/workflows/release.yml` at v0.4.1) builds Windows x64 and
ARM64, Linux x64 (.deb, .rpm, AppImage) and macOS arm64 only. This repository builds from
upstream's source, so it can offer only what upstream builds and supports: **macOS Intel and Linux
ARM are not built by upstream**, and this add-on does not add platforms upstream does not have.
Windows ARM64 is built by upstream and could be added here; it is not yet (libmimusb's ARM64
Windows library is cross-built there and not run in CI). Each package is one archive: a window `installer`, the program in `bin/` with a
`manifest.txt`, the example config, a README and the licenses.

## Installing

`installer/` is one C++20 program (Dear ImGui + GLFW, linked statically) for all three systems.
The window is fixed (dark cards, one accent colour, the palette of aoahid_player); it has no
settings of its own. `src/core.cpp` holds what it does and has no system call in it;
`src/platform.cpp` holds where things live and how MoonlightWeb is stopped.

- `bin/manifest.txt` (written by `tools/make_manifest.py`): the release version, then the SHA-256 and
  path of every regular file in `bin/`. The installer refuses a `bin/` that does not match it
  or a manifest path that leaves `bin/`, and checks the installed files against it after copying.
- **Windows** merges file by file into `C:\Program Files\MoonlightWeb` (the folder holds files this
  add-on knows nothing about). **Linux and macOS** replace one unit as a whole: the AppImage, or
  `MoonlightWeb.app`.
- A file in place is the add-on's only if it is byte for byte what was installed last time (the
  record is `mimusb-install.txt` in the install folder on Windows, `install.txt` under
  `~/.local/share/moonlight-web-mimusb` or `~/Library/Application Support/moonlight-web-mimusb`
  elsewhere). Any other file is an official one (maybe a newer build that upstream's updater put
  there) and becomes the one `*.official` copy, so undo never brings back an older official build.
- The installer does not read the installed official build's version: it replaces the build as a whole,
  so no version has to match. It shows the release's version, and whether the installed files are
  the add-on's.
- The Windows installer runs elevated (its manifest asks for it). It trusts the folder it is run
  from, like any downloaded installer: take the package from a release of this repository.
  Unlike the scripts it replaced, it does not stage an extraction under `%ProgramData%`; there is no
  archive to extract, and the check against `manifest.txt` is for damage, not for an attacker who
  can already write next to the installer.
- `mimusb.json` is edited as text, key by key (`port`, `baud`, `usb.vid`, `usb.pid`, `manufacturer`,
  `product`, `serial`); nothing is filled in. A file with two boards is not edited. The old file is
  kept as `mimusb.json.bak`.

Upstream's own tasks (autostart, stream worker) keep working: they already point at the install
folder. Upstream's own installer restores the official build; run the add-on's installer again after it.

**Versions.** The release carries upstream's tag; the installer has no version of its own. A change
to `installer/` is rolled out by `build.yml` through the tree id of `installer/` written into the
notes (`installer: <id>`): when it differs, the newest release is rebuilt and its files replaced.

## Known limits

- Absolute pointer positions (touch trackpad) become relative moves from the
  previous position; the target's pointer acceleration applies.
- Text typing (`Utf8Text`, `CharDown`) is US-layout ASCII. HID names key
  positions; the target's layout decides the character.
- JIS: the Yen, Ro, Kana, Henkan and Muhenkan keys are sent; a full positional
  remap of the other JIS keys is not done.
- Only verified on Windows with one board. Several boards, Linux and macOS are
  written to the same design and compiled, not exercised on hardware.

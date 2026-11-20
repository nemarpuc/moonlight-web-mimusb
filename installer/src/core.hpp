// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimusb contributors
//
// What the installer does, with no window and no platform call in it: read the
// release's manifest, put the add-on build in place of the official one, and
// put the official one back.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace mwi {

namespace fs = std::filesystem;

// A path as UTF-8 text, and back.
[[nodiscard]] std::string utf8(const fs::path& path);
[[nodiscard]] fs::path from_utf8(const std::string& text);

struct Entry {
    std::string rel; // below bin/, with "/" as the separator
    std::string sha256;
};

// bin/manifest.txt: a line "version<TAB><text>", then one line per file,
// "file<TAB><sha256><TAB><relative path>".
struct Manifest {
    std::string version;
    std::vector<Entry> files;
};

[[nodiscard]] bool load_manifest(const fs::path& bin, Manifest& out, std::string& error);

// Every file the manifest names is in bin/ and matches its hash.
[[nodiscard]] bool verify_bin(const fs::path& bin, const Manifest& manifest, std::string& error);

enum class Mode {
    // Each file is put among the official ones (Windows: an install folder
    // that holds files this add-on knows nothing about).
    Merge,
    // Each top-level entry of bin/ replaces the same entry of `dir` as a whole
    // (Linux: the AppImage; macOS: MoonlightWeb.app).
    Units,
};

struct Plan {
    fs::path bin;   // the release's bin/ folder
    fs::path dir;   // where the official build lives
    fs::path state; // what this installer did, kept so it can be undone
    Mode mode = Mode::Merge;
    // Something in `dir` that exists once the official build is installed.
    std::string probe;
};

enum class State {
    Absent,   // no official build at the target
    Official, // an official build, not touched
    AddOn,    // this installer's build, byte for byte
};

[[nodiscard]] State current_state(const Plan& plan);

using Log = std::function<void(const std::string&)>;

// Both stop at the first error (with `error` set) and leave `.official` copies
// where they are, so running them again is safe.
[[nodiscard]] bool install(const Plan& plan, const Manifest& manifest, const Log& log,
                           std::string& error);
[[nodiscard]] bool rollback(const Plan& plan, const Log& log, std::string& error);

} // namespace mwi

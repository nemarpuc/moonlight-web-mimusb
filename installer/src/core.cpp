// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimusb contributors
#include "core.hpp"

#include "sha256.hpp"

#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace mwi {
namespace {

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (;;) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            parts.push_back(line.substr(start));
            return parts;
        }
        parts.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
}

bool is_hex_digest(const std::string& text) {
    if (text.size() != 64)
        return false;
    for (const char c : text)
        if ((c < '0' || c > '9') && (c < 'a' || c > 'f'))
            return false;
    return true;
}

// A relative path that stays below the folder it is joined to.
bool is_safe_rel(const std::string& rel) {
    if (rel.empty() || rel.front() == '/' || rel.find('\\') != std::string::npos ||
        rel.find(':') != std::string::npos)
        return false;
    std::size_t start = 0;
    while (start <= rel.size()) {
        const std::size_t slash = rel.find('/', start);
        const std::string part =
            rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part.empty() || part == "." || part == "..")
            return false;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return true;
}

std::string unit_of(const std::string& rel) { return rel.substr(0, rel.find('/')); }

// What the installer recorded: files it added (nothing official was there) and
// the hash of every file it put in place.
struct Record {
    bool present = false;
    std::set<std::string> added;
    std::map<std::string, std::string> hashes;
};

Record load_record(const fs::path& path) {
    Record record;
    std::ifstream in(path);
    if (!in)
        return record;
    record.present = true;
    std::string line;
    while (std::getline(in, line)) {
        const auto parts = split_tabs(line);
        if (parts.size() == 2 && parts[0] == "A")
            record.added.insert(parts[1]);
        else if (parts.size() == 3 && parts[0] == "H")
            record.hashes[parts[2]] = parts[1];
    }
    return record;
}

bool save_record(const fs::path& path, const Record& record, std::string& error) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    const fs::path temp = path.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        for (const std::string& rel : record.added)
            out << "A\t" << rel << '\n';
        for (const auto& [rel, sha] : record.hashes)
            out << "H\t" << sha << '\t' << rel << '\n';
        out.flush();
        if (!out) {
            error = "cannot write " + utf8(temp);
            return false;
        }
    }
    fs::rename(temp, path, ec);
    if (ec) {
        error = "cannot write " + utf8(path) + ": " + ec.message();
        return false;
    }
    return true;
}

bool file_matches(const fs::path& path, const std::string& sha) {
    std::string ignored;
    return fs::is_regular_file(path) && sha256_file(path, ignored) == sha;
}

bool fail(std::string& error, const std::string& text, const std::error_code& ec) {
    error = text + ": " + ec.message();
    return false;
}

bool remove_all_quiet(const fs::path& path, std::string& error) {
    std::error_code ec;
    fs::remove_all(path, ec);
    return !ec || fail(error, "cannot remove " + utf8(path), ec);
}

bool move_aside(const fs::path& path, std::string& error) {
    const fs::path official = path.string() + ".official";
    if (!remove_all_quiet(official, error))
        return false;
    std::error_code ec;
    fs::rename(path, official, ec);
    return !ec || fail(error, "cannot keep " + utf8(path) + " as " + utf8(official), ec);
}

bool restore_official(const fs::path& path, bool& restored, std::string& error) {
    const fs::path official = path.string() + ".official";
    restored = false;
    if (!fs::exists(fs::symlink_status(official)))
        return true;
    if (!remove_all_quiet(path, error))
        return false;
    std::error_code ec;
    fs::rename(official, path, ec);
    if (ec)
        return fail(error, "cannot restore " + utf8(path), ec);
    restored = true;
    return true;
}

// The units of the manifest, each with the files it holds.
std::map<std::string, std::vector<const Entry*>> units_of(const Manifest& manifest) {
    std::map<std::string, std::vector<const Entry*>> units;
    for (const Entry& entry : manifest.files)
        units[unit_of(entry.rel)].push_back(&entry);
    return units;
}

} // namespace

std::string utf8(const fs::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

fs::path from_utf8(const std::string& text) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

bool load_manifest(const fs::path& bin, Manifest& out, std::string& error) {
    const fs::path path = bin / "manifest.txt";
    std::ifstream in(path);
    if (!in) {
        error = "no manifest.txt in " + utf8(bin) + " (run the installer from the unpacked release)";
        return false;
    }
    Manifest manifest;
    std::set<std::string> seen;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        const auto parts = split_tabs(line);
        if (parts.size() == 2 && parts[0] == "version") {
            manifest.version = parts[1];
        } else if (parts.size() == 3 && parts[0] == "file") {
            if (!is_hex_digest(parts[1]) || !is_safe_rel(parts[2]) || !seen.insert(parts[2]).second) {
                error = "manifest.txt has a bad line: " + line;
                return false;
            }
            manifest.files.push_back({parts[2], parts[1]});
        } else {
            error = "manifest.txt has a line it does not know: " + line;
            return false;
        }
    }
    if (manifest.version.empty() || manifest.files.empty()) {
        error = "manifest.txt has no version or no files";
        return false;
    }
    out = std::move(manifest);
    return true;
}

bool verify_bin(const fs::path& bin, const Manifest& manifest, std::string& error) {
    for (const Entry& entry : manifest.files) {
        const fs::path path = bin / from_utf8(entry.rel);
        if (!fs::is_regular_file(path)) {
            error = "missing from bin/: " + entry.rel;
            return false;
        }
        std::string read_error;
        const std::string sha = sha256_file(path, read_error);
        if (sha.empty()) {
            error = read_error;
            return false;
        }
        if (sha != entry.sha256) {
            error = "bin/" + entry.rel + " is not the file the release was built with";
            return false;
        }
    }
    return true;
}

State current_state(const Plan& plan) {
    if (!fs::exists(fs::symlink_status(plan.dir / from_utf8(plan.probe))))
        return State::Absent;
    const Record record = load_record(plan.state);
    if (!record.present || record.hashes.empty())
        return State::Official;
    for (const auto& [rel, sha] : record.hashes)
        if (!file_matches(plan.dir / from_utf8(rel), sha))
            return State::Official;
    return State::AddOn;
}

bool install(const Plan& plan, const Manifest& manifest, const Log& log, std::string& error) {
    if (plan.mode == Mode::Merge && !fs::exists(plan.dir / from_utf8(plan.probe))) {
        error = "the official build is not installed in " + utf8(plan.dir) +
                " (install it first, or choose its folder)";
        return false;
    }
    Record record = load_record(plan.state);
    std::error_code ec;
    fs::create_directories(plan.dir, ec);
    if (ec)
        return fail(error, "cannot create " + utf8(plan.dir), ec);

    // A file in place is ours only if it is what was installed last time. Any
    // other file is an official one (maybe a newer build put there by upstream's
    // own updater) and becomes the `.official` copy: rolling back must never
    // bring back an older official version.
    const auto is_ours = [&](const std::string& rel) {
        const auto found = record.hashes.find(rel);
        return found != record.hashes.end() && file_matches(plan.dir / from_utf8(rel), found->second);
    };

    if (plan.mode == Mode::Merge) {
        for (const Entry& entry : manifest.files) {
            const fs::path source = plan.bin / from_utf8(entry.rel);
            const fs::path dest = plan.dir / from_utf8(entry.rel);
            fs::create_directories(dest.parent_path(), ec);
            if (fs::exists(fs::symlink_status(dest))) {
                if (!record.added.contains(entry.rel) && !is_ours(entry.rel)) {
                    fs::copy_file(dest, dest.string() + ".official",
                                  fs::copy_options::overwrite_existing, ec);
                    if (ec)
                        return fail(error, "cannot keep " + utf8(dest) + " as .official", ec);
                }
            } else {
                record.added.insert(entry.rel);
            }
            fs::copy_file(source, dest, fs::copy_options::overwrite_existing, ec);
            if (ec)
                return fail(error, "cannot copy " + entry.rel, ec);
            record.hashes[entry.rel] = entry.sha256;
        }
        log("copied " + std::to_string(manifest.files.size()) + " files");
    } else {
        for (const auto& [unit, entries] : units_of(manifest)) {
            const fs::path source = plan.bin / from_utf8(unit);
            const fs::path dest = plan.dir / from_utf8(unit);
            if (fs::exists(fs::symlink_status(dest))) {
                bool ours = true;
                for (const Entry* entry : entries)
                    ours = ours && is_ours(entry->rel);
                if (!ours) {
                    if (!move_aside(dest, error))
                        return false;
                    log("kept the official " + unit + " as " + unit + ".official");
                } else if (!remove_all_quiet(dest, error)) {
                    return false;
                }
            } else {
                record.added.insert(unit);
            }
            fs::copy(source, dest, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
            if (ec)
                return fail(error, "cannot copy " + unit, ec);
            for (const Entry* entry : entries)
                record.hashes[entry->rel] = entry->sha256;
            log("installed " + unit);
        }
    }

    for (const Entry& entry : manifest.files)
        if (!file_matches(plan.dir / from_utf8(entry.rel), entry.sha256)) {
            error = utf8(plan.dir / from_utf8(entry.rel)) + " is not the new file after copying";
            return false;
        }
    return save_record(plan.state, record, error);
}

bool rollback(const Plan& plan, const Log& log, std::string& error) {
    const Record record = load_record(plan.state);
    if (!record.present) {
        error = "this installer did not install anything here (no record at " + utf8(plan.state) + ")";
        return false;
    }
    std::set<std::string> targets;
    for (const auto& [rel, sha] : record.hashes)
        targets.insert(plan.mode == Mode::Units ? unit_of(rel) : rel);
    for (const std::string& rel : targets) {
        const fs::path path = plan.dir / from_utf8(rel);
        bool restored = false;
        if (!restore_official(path, restored, error))
            return false;
        if (restored) {
            log("restored " + rel);
        } else if (record.added.contains(rel)) {
            if (!remove_all_quiet(path, error))
                return false;
            log("removed " + rel);
        } else {
            log("no official copy of " + rel + " was kept; left as it is");
        }
    }
    std::error_code ec;
    fs::remove(plan.state, ec);
    return true;
}

} // namespace mwi

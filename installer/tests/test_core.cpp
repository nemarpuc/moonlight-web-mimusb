// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
#include "config.hpp"
#include "core.hpp"
#include "sha256.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace mwi;

namespace {

int failures = 0;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);      \
            ++failures;                                                                        \
        }                                                                                      \
    } while (false)

void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string sha_of(const std::string& text) {
    Sha256 sha;
    sha.update(text.data(), text.size());
    return sha.hex();
}

// bin/ with the given files and a manifest for them.
void make_bin(const fs::path& bin, const std::vector<std::pair<std::string, std::string>>& files) {
    std::string manifest = "version\tv9.9.9\n";
    for (const auto& [rel, text] : files) {
        write(bin / rel, text);
        manifest += "file\t" + sha_of(text) + "\t" + rel + "\n";
    }
    write(bin / "manifest.txt", manifest);
}

void test_sha256() {
    CHECK(sha_of("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha_of("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha_of(std::string(1000, 'a')) ==
          "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}

void test_manifest(const fs::path& root) {
    const fs::path bin = root / "m";
    make_bin(bin, {{"a.txt", "alpha"}, {"sub/b.txt", "beta"}});
    Manifest manifest;
    std::string error;
    CHECK(load_manifest(bin, manifest, error));
    CHECK(manifest.version == "v9.9.9" && manifest.files.size() == 2);
    CHECK(verify_bin(bin, manifest, error));
    write(bin / "a.txt", "tampered");
    CHECK(!verify_bin(bin, manifest, error));

    write(bin / "manifest.txt", "version\tv1\nfile\t" + sha_of("x") + "\t../escape\n");
    CHECK(!load_manifest(bin, manifest, error));
    write(bin / "manifest.txt", "version\tv1\nfile\t" + sha_of("x") + "\t/abs\n");
    CHECK(!load_manifest(bin, manifest, error));
}

Plan plan_for(const fs::path& root, const Mode mode, const std::string& probe) {
    Plan plan;
    plan.bin = root / "bin";
    plan.dir = root / "target";
    plan.state = root / "state" / "install.txt";
    plan.mode = mode;
    plan.probe = probe;
    return plan;
}

void test_merge(const fs::path& root) {
    const Plan plan = plan_for(root, Mode::Merge, "app.exe");
    make_bin(plan.bin, {{"app.exe", "ADDON"}, {"lib/new.dll", "NEWLIB"}});
    write(plan.dir / "app.exe", "OFFICIAL");
    write(plan.dir / "other.dll", "UNRELATED");
    Manifest manifest;
    std::string error;
    CHECK(load_manifest(plan.bin, manifest, error));
    const Log log = [](const std::string&) {};

    CHECK(current_state(plan) == State::Official);
    CHECK(install(plan, manifest, log, error));
    CHECK(current_state(plan) == State::AddOn);
    CHECK(read(plan.dir / "app.exe") == "ADDON");
    CHECK(read(plan.dir / "app.exe.official") == "OFFICIAL");
    CHECK(read(plan.dir / "other.dll") == "UNRELATED");

    // Installing again must not turn our own file into the "official" copy.
    CHECK(install(plan, manifest, log, error));
    CHECK(read(plan.dir / "app.exe.official") == "OFFICIAL");

    // Upstream's updater put a newer official file in: it becomes the kept copy.
    write(plan.dir / "app.exe", "OFFICIAL2");
    CHECK(current_state(plan) == State::Official);
    CHECK(install(plan, manifest, log, error));
    CHECK(read(plan.dir / "app.exe.official") == "OFFICIAL2");

    CHECK(rollback(plan, log, error));
    CHECK(read(plan.dir / "app.exe") == "OFFICIAL2");
    CHECK(!fs::exists(plan.dir / "lib/new.dll"));
    CHECK(read(plan.dir / "other.dll") == "UNRELATED");
    CHECK(current_state(plan) == State::Official);
    CHECK(!rollback(plan, log, error)); // nothing recorded any more

    // No official build: Merge refuses.
    fs::remove(plan.dir / "app.exe");
    CHECK(!install(plan, manifest, log, error));
}

void test_units(const fs::path& root) {
    const Plan plan = plan_for(root, Mode::Units, "App.bundle");
    make_bin(plan.bin, {{"App.bundle/exe", "ADDON"}, {"App.bundle/res/x", "RES"}});
    write(plan.dir / "App.bundle/exe", "OFFICIAL");
    Manifest manifest;
    std::string error;
    CHECK(load_manifest(plan.bin, manifest, error));
    const Log log = [](const std::string&) {};

    CHECK(install(plan, manifest, log, error));
    CHECK(read(plan.dir / "App.bundle/exe") == "ADDON");
    CHECK(read(plan.dir / "App.bundle.official/exe") == "OFFICIAL");
    CHECK(current_state(plan) == State::AddOn);
    CHECK(install(plan, manifest, log, error));
    CHECK(read(plan.dir / "App.bundle.official/exe") == "OFFICIAL");
    CHECK(rollback(plan, log, error));
    CHECK(read(plan.dir / "App.bundle/exe") == "OFFICIAL");
    CHECK(!fs::exists(plan.dir / "App.bundle/res"));

    // Nothing official before: rollback removes the add-on.
    fs::remove_all(plan.dir / "App.bundle");
    CHECK(install(plan, manifest, log, error));
    CHECK(rollback(plan, log, error));
    CHECK(!fs::exists(plan.dir / "App.bundle"));
}

const char* example = R"({
  "enabled": true,
  "devices": [ { "port": "auto", "baud": 0,
    "usb": { "vid": "0x1209", "pid": "0x0001", "manufacturer": "A \"B\"", "product": "P", "serial": "",
             "max_power_ma": 100 },
    "keyboard": { "name": "", "layout": "us" } } ]
})";

void test_config() {
    Fields fields;
    std::string error;
    CHECK(read_fields(example, fields, error));
    CHECK(fields.port == "auto" && fields.baud == "0" && fields.vid == "0x1209");
    CHECK(fields.manufacturer == "A \"B\"" && fields.serial.empty());
    CHECK(check_fields(fields).empty());

    fields.vid = "0xCAFE";
    fields.product = "My $1 board";
    fields.serial = "S\\N";
    std::string out;
    CHECK(write_fields(example, fields, out, error));
    Fields back;
    CHECK(read_fields(out, back, error));
    CHECK(back.vid == "0xCAFE" && back.product == "My $1 board" && back.serial == "S\\N");
    CHECK(out.find("\"max_power_ma\": 100") != std::string::npos); // the rest is untouched
    CHECK(out.find("\"layout\": \"us\"") != std::string::npos);

    Fields bad = fields;
    bad.vid = "1209";
    CHECK(!check_fields(bad).empty());
    bad = fields;
    bad.port = "COM3";
    bad.baud = "0";
    CHECK(!check_fields(bad).empty());

    // Two boards: refuse rather than edit the first and hide the second.
    const std::string two = std::string(example) + std::string(example);
    CHECK(!read_fields(two, back, error));
    CHECK(!read_fields("{}", back, error));
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / ("mwi-test-" + std::to_string(std::rand()));
    fs::remove_all(root);
    test_sha256();
    test_manifest(root / "a");
    test_merge(root / "b");
    test_units(root / "c");
    test_config();
    fs::remove_all(root);
    if (failures == 0)
        std::puts("all passed");
    return failures == 0 ? 0 : 1;
}

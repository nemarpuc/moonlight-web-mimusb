// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
#pragma once

#include "config.hpp"
#include "core.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace mwi {

// What the window shows and the actions behind its buttons. The slow steps run
// on a worker thread; everything the worker touches is behind `mutex`.
struct App {
    Plan plan;
    fs::path release_dir;
    std::string dir_text; // the install folder as typed

    Manifest manifest;
    std::string manifest_error; // empty when bin/ has a good manifest

    std::mutex mutex;
    State state = State::Absent;
    std::vector<std::string> log;
    std::string last_error;
    std::atomic<bool> busy{false};

    // mimhid.json
    fs::path config_path;
    std::string config_text; // what is on disk, or the example when there is no file
    bool config_exists = false;
    Fields fields;
    std::string config_error; // empty when the keys were read
    std::string config_note;  // the last save's outcome
};

void init(App& app);
// Re-reads the state (hashes every installed file: run it off the UI thread).
void refresh_state(App& app);
void start_install(App& app);
void start_rollback(App& app);
void save_config(App& app);
void reload_config(App& app);

} // namespace mwi

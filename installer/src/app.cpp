// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimusb contributors
#include "app.hpp"

#include "platform.hpp"

#include <fstream>
#include <sstream>
#include <thread>

namespace mwi {
namespace {

void add_log(App& app, const std::string& line) {
    const std::lock_guard lock(app.mutex);
    app.log.push_back(line);
}

bool read_text(const fs::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

// Runs `work` off the UI thread; one at a time.
template <typename Work> void run_job(App& app, Work work) {
    if (app.busy.exchange(true))
        return;
    std::thread([&app, work = std::move(work)] {
        std::string error;
        const bool ok = work(error);
        if (!ok) {
            add_log(app, "error: " + error);
            const std::lock_guard lock(app.mutex);
            app.last_error = error;
        }
        refresh_state(app);
        app.busy = false;
    }).detach();
}

} // namespace

void init(App& app) {
    app.release_dir = program_dir();
    app.plan = default_plan(app.release_dir);
    app.dir_text = utf8(app.plan.dir);
    if (!load_manifest(app.plan.bin, app.manifest, app.manifest_error))
        app.manifest = {};
    reload_config(app);
}

void refresh_state(App& app) {
    Plan plan;
    {
        const std::lock_guard lock(app.mutex);
        plan = app.plan;
    }
    const State state = current_state(plan);
    const std::lock_guard lock(app.mutex);
    app.state = state;
}

void start_install(App& app) {
    Plan plan;
    {
        const std::lock_guard lock(app.mutex);
        app.plan.dir = from_utf8(app.dir_text);
        plan = app.plan;
        app.last_error.clear();
    }
    const Manifest manifest = app.manifest;
    run_job(app, [&app, plan, manifest](std::string& error) {
        add_log(app, "checking bin/ against its manifest");
        if (!verify_bin(plan.bin, manifest, error))
            return false;
        add_log(app, "stopping MoonlightWeb");
        stop_app();
        const auto log = [&app](const std::string& line) { add_log(app, line); };
        if (!install(plan, manifest, log, error))
            return false;
        after_install(plan);
        add_log(app, "done: the add-on build " + manifest.version + " is installed");
        return true;
    });
}

void start_rollback(App& app) {
    Plan plan;
    {
        const std::lock_guard lock(app.mutex);
        app.plan.dir = from_utf8(app.dir_text);
        plan = app.plan;
        app.last_error.clear();
    }
    run_job(app, [&app, plan](std::string& error) {
        add_log(app, "stopping MoonlightWeb");
        stop_app();
        const auto log = [&app](const std::string& line) { add_log(app, line); };
        if (!rollback(plan, log, error))
            return false;
        after_rollback(plan);
        add_log(app, "done: the official build is back");
        return true;
    });
}

void reload_config(App& app) {
    app.config_path = config_dir() / "mimusb.json";
    app.config_exists = read_text(app.config_path, app.config_text);
    std::string example;
    if (!app.config_exists) {
        if (!read_text(app.release_dir / "mimusb.example.json", app.config_text)) {
            app.config_text.clear();
            app.config_error = "mimusb.example.json is not beside the installer";
            return;
        }
    }
    app.config_error.clear();
    if (!read_fields(app.config_text, app.fields, app.config_error))
        app.fields = {};
}

void save_config(App& app) {
    app.config_note.clear();
    const std::string problem = check_fields(app.fields);
    if (!problem.empty()) {
        app.config_note = "not saved: " + problem;
        return;
    }
    std::string text;
    std::string error;
    if (!write_fields(app.config_text, app.fields, text, error)) {
        app.config_note = "not saved: " + error;
        return;
    }
    std::error_code ec;
    fs::create_directories(app.config_path.parent_path(), ec);
    if (app.config_exists) {
        fs::copy_file(app.config_path, app.config_path.string() + ".bak",
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            app.config_note = "not saved: cannot keep mimusb.json.bak: " + ec.message();
            return;
        }
    }
    {
        std::ofstream out(app.config_path, std::ios::binary | std::ios::trunc);
        out << text;
        if (!out) {
            app.config_note = "not saved: cannot write " + utf8(app.config_path);
            return;
        }
    }
    app.config_text = text;
    app.config_exists = true;
    app.config_note = "saved. Restart MoonlightWeb to use it";
}

} // namespace mwi

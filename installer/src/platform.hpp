// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
//
// The parts that differ by system: where the official build lives, how to stop
// it, and what is added around the files.
#pragma once

#include "core.hpp"

namespace mwi {

// The folder this program was started from.
[[nodiscard]] fs::path program_dir();

// Where MoonlightWeb keeps settings.json, and so mimhid.json.
[[nodiscard]] fs::path config_dir();

// The plan for this system; `dir` is where the official build is expected.
[[nodiscard]] Plan default_plan(const fs::path& release_dir);

// Ends a running MoonlightWeb so its files can be replaced.
void stop_app();

// Menu entry, quarantine flag, executable bit: what the files alone do not say.
void after_install(const Plan& plan);
void after_rollback(const Plan& plan);

} // namespace mwi

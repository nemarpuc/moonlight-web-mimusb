// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
//
// The few keys of mimhid.json the window edits. The file is edited as text, key
// by key, so everything else in it (the long list of timeouts and ranges) stays
// exactly as it was; nothing here fills in a key that is missing.
#pragma once

#include <string>

namespace mwi {

struct Fields {
    std::string port;
    std::string baud; // a whole number as text, so a half-typed value can be shown
    std::string vid;  // "0x1209"
    std::string pid;
    std::string manufacturer;
    std::string product;
    std::string serial;
};

// Reads the keys out of a mimhid.json (or the example). Fails if one is missing
// or the file describes more than one board (edit that one by hand).
[[nodiscard]] bool read_fields(const std::string& json, Fields& out, std::string& error);

// What is wrong with the values, or an empty string.
[[nodiscard]] std::string check_fields(const Fields& fields);

// The text with the keys replaced. Fails like read_fields does.
[[nodiscard]] bool write_fields(const std::string& json, const Fields& fields, std::string& out,
                                std::string& error);

} // namespace mwi

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
#include "config.hpp"

#include <cctype>
#include <regex>

namespace mwi {
namespace {

std::string string_pattern(const std::string& key) {
    return "(\"" + key + "\"\\s*:\\s*)\"((?:[^\"\\\\]|\\\\.)*)\"";
}

std::string number_pattern(const std::string& key) { return "(\"" + key + "\"\\s*:\\s*)(-?[0-9]+)"; }

std::size_t count_matches(const std::string& json, const std::regex& pattern) {
    return static_cast<std::size_t>(std::distance(
        std::sregex_iterator(json.begin(), json.end(), pattern), std::sregex_iterator()));
}

// The value of a key that must occur exactly once.
bool find_value(const std::string& json, const std::string& key, const bool is_string,
                std::string& value, std::string& error) {
    const std::regex pattern(is_string ? string_pattern(key) : number_pattern(key));
    const std::size_t count = count_matches(json, pattern);
    if (count == 0) {
        error = "the file has no \"" + key + "\" key";
        return false;
    }
    if (count > 1) {
        error = "the file has more than one \"" + key +
                "\": it describes several boards, so edit it by hand";
        return false;
    }
    std::smatch match;
    std::regex_search(json, match, pattern);
    value = match[2].str();
    return true;
}

std::string json_escape(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string json_unescape(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size())
            ++i;
        out.push_back(text[i]);
    }
    return out;
}

// The key occurs exactly once (read_fields has checked it).
void replace_value(std::string& json, const std::string& key, const bool is_string,
                   const std::string& value) {
    const std::regex pattern(is_string ? string_pattern(key) : number_pattern(key));
    std::smatch match;
    if (std::regex_search(json, match, pattern))
        json.replace(static_cast<std::size_t>(match.position(2)),
                     static_cast<std::size_t>(match.length(2)),
                     is_string ? json_escape(value) : value);
}

bool is_id(const std::string& text) {
    if (text.size() < 3 || text.size() > 6 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X'))
        return false;
    for (std::size_t i = 2; i < text.size(); ++i)
        if (std::isxdigit(static_cast<unsigned char>(text[i])) == 0)
            return false;
    return true;
}

bool has_control(const std::string& text) {
    for (const char c : text)
        if (static_cast<unsigned char>(c) < 0x20)
            return true;
    return false;
}

constexpr std::size_t max_string_bytes = 126; // a USB string descriptor holds 126 UTF-16 units

} // namespace

bool read_fields(const std::string& json, Fields& out, std::string& error) {
    Fields fields;
    std::string raw;
    const struct {
        const char* key;
        bool is_string;
        std::string* target;
    } keys[] = {{"port", true, &fields.port},         {"baud", false, &fields.baud},
                {"vid", true, &fields.vid},           {"pid", true, &fields.pid},
                {"manufacturer", true, &fields.manufacturer},
                {"product", true, &fields.product},   {"serial", true, &fields.serial}};
    for (const auto& key : keys) {
        if (!find_value(json, key.key, key.is_string, raw, error))
            return false;
        *key.target = key.is_string ? json_unescape(raw) : raw;
    }
    out = std::move(fields);
    return true;
}

std::string check_fields(const Fields& fields) {
    if (fields.port.empty())
        return "port is empty (\"auto\" takes the first board)";
    if (fields.baud.empty() || fields.baud.size() > 9 ||
        fields.baud.find_first_not_of("0123456789") != std::string::npos)
        return "baud must be a whole number";
    if (std::stoul(fields.baud) == 0 && fields.port != "auto")
        return "a baud of 0 works only with port \"auto\"";
    if (!is_id(fields.vid))
        return "vid must look like 0x1209";
    if (!is_id(fields.pid))
        return "pid must look like 0x0001";
    for (const std::string* text : {&fields.port, &fields.manufacturer, &fields.product, &fields.serial})
        if (has_control(*text))
            return "text fields cannot hold control characters";
    for (const std::string* text : {&fields.manufacturer, &fields.product, &fields.serial})
        if (text->size() > max_string_bytes)
            return "a USB string holds at most 126 characters";
    if (fields.manufacturer.empty() || fields.product.empty())
        return "manufacturer and product cannot be empty";
    return {};
}

bool write_fields(const std::string& json, const Fields& fields, std::string& out,
                  std::string& error) {
    Fields ignored;
    if (!read_fields(json, ignored, error))
        return false;
    std::string text = json;
    replace_value(text, "port", true, fields.port);
    replace_value(text, "baud", false, fields.baud);
    replace_value(text, "vid", true, fields.vid);
    replace_value(text, "pid", true, fields.pid);
    replace_value(text, "manufacturer", true, fields.manufacturer);
    replace_value(text, "product", true, fields.product);
    replace_value(text, "serial", true, fields.serial);
    out = std::move(text);
    return true;
}

} // namespace mwi

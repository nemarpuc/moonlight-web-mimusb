/*
 * MoonlightWeb physical-HID add-on (unofficial).
 * Copyright (C) 2026 the moonlight-web-mimusb contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "MimUsbConfig.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace mw::native::input {

namespace {

/// Just enough JSON for a settings file: objects, arrays, strings, numbers,
/// booleans, null. No dependency is worth adding for a dozen keys.
struct Value
{
    enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Value> arr;
    std::map<std::string, Value> obj;
};

class Parser
{
public:
    explicit Parser(const std::string& text) : m_S(text) {}

    bool parse(Value& out, std::string& error)
    {
        skip();
        if (!value(out)) {
            error = m_Error.empty() ? "invalid JSON" : m_Error;
            return false;
        }
        skip();
        if (m_I != m_S.size()) {
            error = "trailing characters after the JSON value";
            return false;
        }
        return true;
    }

private:
    const std::string& m_S;
    size_t m_I = 0;
    std::string m_Error;
    int m_Depth = 0;

    void skip()
    {
        while (m_I < m_S.size() && std::isspace(static_cast<unsigned char>(m_S[m_I]))) ++m_I;
    }
    bool fail(const char* why)
    {
        if (m_Error.empty()) m_Error = std::string(why) + " at offset " + std::to_string(m_I);
        return false;
    }
    bool literal(const char* word)
    {
        size_t n = 0;
        while (word[n]) ++n;
        if (m_S.compare(m_I, n, word) != 0) return fail("unexpected token");
        m_I += n;
        return true;
    }
    bool string(std::string& out)
    {
        if (m_I >= m_S.size() || m_S[m_I] != '"') return fail("expected a string");
        ++m_I;
        out.clear();
        while (m_I < m_S.size()) {
            const char c = m_S[m_I++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (m_I >= m_S.size()) break;
            const char e = m_S[m_I++];
            switch (e) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'u':
                // Settings here are ASCII; keep a \uXXXX below 0x80, drop the rest.
                if (m_I + 4 <= m_S.size()) {
                    const unsigned long cp = std::strtoul(m_S.substr(m_I, 4).c_str(), nullptr, 16);
                    if (cp < 0x80) out.push_back(static_cast<char>(cp));
                    m_I += 4;
                }
                break;
            default: out.push_back(e); break; // " \ /
            }
        }
        return fail("unterminated string");
    }
    bool value(Value& out)
    {
        if (++m_Depth > 16) return fail("nested too deep");
        struct Leave
        {
            int& d;
            ~Leave() { --d; }
        } leave{m_Depth};
        skip();
        if (m_I >= m_S.size()) return fail("unexpected end");
        const char c = m_S[m_I];
        if (c == '{') return object(out);
        if (c == '[') return array(out);
        if (c == '"') {
            out.kind = Value::Kind::String;
            return string(out.str);
        }
        if (c == 't') {
            out.kind = Value::Kind::Bool;
            out.b = true;
            return literal("true");
        }
        if (c == 'f') {
            out.kind = Value::Kind::Bool;
            out.b = false;
            return literal("false");
        }
        if (c == 'n') {
            out.kind = Value::Kind::Null;
            return literal("null");
        }
        char* end = nullptr;
        const double d = std::strtod(m_S.c_str() + m_I, &end);
        if (end == m_S.c_str() + m_I) return fail("unexpected token");
        m_I = static_cast<size_t>(end - m_S.c_str());
        out.kind = Value::Kind::Number;
        out.num = d;
        return true;
    }
    bool array(Value& out)
    {
        out.kind = Value::Kind::Array;
        ++m_I; // [
        skip();
        if (m_I < m_S.size() && m_S[m_I] == ']') {
            ++m_I;
            return true;
        }
        for (;;) {
            out.arr.emplace_back();
            if (!value(out.arr.back())) return false;
            skip();
            if (m_I >= m_S.size()) return fail("unterminated array");
            if (m_S[m_I] == ',') {
                ++m_I;
                continue;
            }
            if (m_S[m_I] == ']') {
                ++m_I;
                return true;
            }
            return fail("expected , or ]");
        }
    }
    bool object(Value& out)
    {
        out.kind = Value::Kind::Object;
        ++m_I; // {
        skip();
        if (m_I < m_S.size() && m_S[m_I] == '}') {
            ++m_I;
            return true;
        }
        for (;;) {
            skip();
            std::string key;
            if (!string(key)) return false;
            skip();
            if (m_I >= m_S.size() || m_S[m_I] != ':') return fail("expected :");
            ++m_I;
            if (!value(out.obj[key])) return false;
            skip();
            if (m_I >= m_S.size()) return fail("unterminated object");
            if (m_S[m_I] == ',') {
                ++m_I;
                continue;
            }
            if (m_S[m_I] == '}') {
                ++m_I;
                return true;
            }
            return fail("expected , or }");
        }
    }
};

const Value* member(const Value& v, const char* key)
{
    if (v.kind != Value::Kind::Object) return nullptr;
    const auto it = v.obj.find(key);
    return it == v.obj.end() ? nullptr : &it->second;
}

/// A number, or a string such as "0x1209" — USB IDs are written in hex.
bool readNumber(const Value& v, const char* key, long long& out)
{
    const Value* m = member(v, key);
    if (!m) return false;
    if (m->kind == Value::Kind::Number) {
        out = static_cast<long long>(m->num);
        return true;
    }
    if (m->kind == Value::Kind::String && !m->str.empty()) {
        char* end = nullptr;
        const long long n = std::strtoll(m->str.c_str(), &end, 0);
        if (end == m->str.c_str() || *end != '\0') return false;
        out = n;
        return true;
    }
    return false;
}

/// Every value of a device is required: a key that is missing, of the wrong kind or out of range
/// stops the parse with its full name in @p error. Nothing is filled in.
class Needs
{
public:
    Needs(std::string base, std::string& error) : m_Base(std::move(base)), m_Error(error) {}

    template <typename T>
    bool number(const Value& v, const char* key, long long lo, long long hi, T& out)
    {
        if (!member(v, key)) return fail(std::string("missing key ") + m_Base + key);
        long long n = 0;
        if (!readNumber(v, key, n)) return fail(m_Base + key + " must be a number");
        if (n < lo || n > hi)
            return fail(m_Base + key + " must be " + std::to_string(lo) + " to " + std::to_string(hi));
        out = static_cast<T>(n);
        return true;
    }
    bool boolean(const Value& v, const char* key, bool& out)
    {
        const Value* m = member(v, key);
        if (!m) return fail(std::string("missing key ") + m_Base + key);
        if (m->kind != Value::Kind::Bool) return fail(m_Base + key + " must be true or false");
        out = m->b;
        return true;
    }
    bool text(const Value& v, const char* key, std::string& out, bool nonEmpty)
    {
        const Value* m = member(v, key);
        if (!m) return fail(std::string("missing key ") + m_Base + key);
        if (m->kind != Value::Kind::String) return fail(m_Base + key + " must be a string");
        if (nonEmpty && m->str.empty()) return fail(m_Base + key + " must not be empty");
        out = m->str;
        return true;
    }
    /// "on_full": what happens to a report when the endpoint's queue is full.
    bool fullPolicy(const Value& v, FullPolicy& out)
    {
        std::string name;
        if (!text(v, "on_full", name, true)) return false;
        if (name == "drop_newest") out = FullPolicy::DropNewest;
        else if (name == "drop_oldest") out = FullPolicy::DropOldest;
        else if (name == "wait") out = FullPolicy::Wait;
        else return fail(m_Base + "on_full must be \"drop_newest\", \"drop_oldest\" or \"wait\"");
        return true;
    }
    const Value* object(const Value& v, const char* key)
    {
        const Value* m = member(v, key);
        if (!m) {
            fail(std::string("missing key ") + m_Base + key);
            return nullptr;
        }
        if (m->kind != Value::Kind::Object) {
            fail(m_Base + key + " must be an object");
            return nullptr;
        }
        return m;
    }
    Needs inside(const char* key) { return Needs(m_Base + key + ".", m_Error); }

private:
    std::string m_Base;
    std::string& m_Error;
    bool fail(const std::string& why)
    {
        if (m_Error.empty()) m_Error = why;
        return false;
    }
};

bool readDevice(const Value& v, const std::string& base, MimUsbDevice& d, std::string& error)
{
    Needs n(base, error);
    if (!n.text(v, "port", d.port, true)) return false;
    if (!n.number(v, "baud", 0, 12000000, d.baud)) return false;
    if (d.baud == 0 && d.port != "auto") {
        error = base + "baud is 0 (the fastest rate discovery found) only for the port \"auto\"";
        return false;
    }

    const Value* link = n.object(v, "link");
    if (!link) return false;
    Needs l = n.inside("link");
    if (!l.number(*link, "handshake_timeout_ms", 1, 60000, d.handshakeTimeoutMs) ||
        !l.number(*link, "control_timeout_ms", 1, 5000, d.controlTimeoutMs) ||
        !l.number(*link, "reconnect_timeout_ms", 1, 600000, d.reconnectTimeoutMs) ||
        !l.number(*link, "send_timeout_ms", 1, 60000, d.sendTimeoutMs) ||
        !l.number(*link, "drain_timeout_ms", 1, 600000, d.drainTimeoutMs) ||
        !l.number(*link, "ready_timeout_ms", 1, 600000, d.readyTimeoutMs) ||
        !l.number(*link, "max_report_bytes", 1, 1024, d.maxReportBytes))
        return false;
    const Value* hid = l.object(*link, "linux_hid");
    if (!hid) return false;
    Needs h = l.inside("linux_hid");
    if (!h.number(*hid, "descriptor_bytes", 1, 0xFFFFFFFFLL, d.linuxDescriptorBytes) ||
        !h.number(*hid, "fields_per_report", 1, 0xFFFFFFFFLL, d.linuxFieldsPerReport) ||
        !h.number(*hid, "global_stack_depth", 1, 0xFFFFFFFFLL, d.linuxGlobalStackDepth) ||
        !h.number(*hid, "report_size_bits", 1, 0xFFFFFFFFLL, d.linuxReportSizeBits) ||
        !h.number(*hid, "usages", 1, 0xFFFFFFFFLL, d.linuxUsages) ||
        !h.number(*hid, "report_data_bits", 1, 0xFFFFFFFFLL, d.linuxReportDataBits))
        return false;

    const Value* usb = n.object(v, "usb");
    if (!usb) return false;
    Needs u = n.inside("usb");
    if (!u.number(*usb, "vid", 1, 0xFFFF, d.vendorId) || !u.number(*usb, "pid", 0, 0xFFFF, d.productId) ||
        !u.text(*usb, "manufacturer", d.manufacturer, false) ||
        !u.text(*usb, "product", d.product, false) || !u.text(*usb, "serial", d.serial, false) ||
        !u.number(*usb, "bcd_device", 0, 0xFFFF, d.bcdDevice) ||
        !u.number(*usb, "bcd_usb", 1, 0xFFFF, d.bcdUsb) ||
        !u.number(*usb, "language_id", 1, 0xFFFF, d.languageId) ||
        !u.number(*usb, "ep0_max_packet_size", 8, 64, d.ep0MaxPacketSize) ||
        !u.number(*usb, "configuration_value", 1, 255, d.configurationValue) ||
        !u.boolean(*usb, "remote_wakeup", d.remoteWakeup) ||
        !u.boolean(*usb, "self_powered", d.selfPowered) ||
        !u.number(*usb, "max_power_ma", 0, 500, d.maxPowerMa))
        return false;

    if (const Value* kb = member(v, "keyboard")) {
        if (kb->kind != Value::Kind::Object) {
            error = base + "keyboard must be an object";
            return false;
        }
        Needs k = n.inside("keyboard");
        d.keyboardEnabled = true; // the object is there: the class is wanted
        if (member(*kb, "enabled") && !k.boolean(*kb, "enabled", d.keyboardEnabled)) return false;
        if (d.keyboardEnabled) {
            std::string layout;
            if (!k.text(*kb, "layout", layout, true)) return false;
            if (layout == "jis" || layout == "JIS") d.keyboardJis = true;
            else if (layout == "us" || layout == "US") d.keyboardJis = false;
            else {
                error = base + "keyboard.layout must be \"us\" or \"jis\"";
                return false;
            }
            if (!k.boolean(*kb, "boot", d.keyboardBoot) ||
                !k.number(*kb, "poll_ms", 1, 255, d.keyboardPollMs) ||
                !k.number(*kb, "queue_depth", 1, 255, d.keyboardQueueDepth) ||
                !k.fullPolicy(*kb, d.keyboardOnFull) ||
                !k.text(*kb, "name", d.keyboardName, false) ||
                !k.number(*kb, "country", 0, 35, d.keyboardCountry) ||
                !k.number(*kb, "bcd_hid", 1, 0xFFFF, d.keyboardBcdHid) ||
                !k.number(*kb, "endpoint_address", 0x81, 0x8F, d.keyboardEndpointAddress) ||
                !k.number(*kb, "max_packet", 1, 1024, d.keyboardMaxPacket) ||
                !k.number(*kb, "usage_minimum", 0, 0xFFFF, d.keyboardUsageMinimum) ||
                !k.number(*kb, "usage_maximum", 0, 0xFFFF, d.keyboardUsageMaximum))
                return false;
        }
    }
    if (const Value* ms = member(v, "mouse")) {
        if (ms->kind != Value::Kind::Object) {
            error = base + "mouse must be an object";
            return false;
        }
        Needs m = n.inside("mouse");
        d.mouseEnabled = true;
        if (member(*ms, "enabled") && !m.boolean(*ms, "enabled", d.mouseEnabled)) return false;
        if (d.mouseEnabled) {
            if (!m.number(*ms, "buttons", 1, 5, d.mouseButtons) || !m.boolean(*ms, "wheel", d.mouseWheel) ||
                !m.boolean(*ms, "pan", d.mousePan) || !m.boolean(*ms, "boot", d.mouseBoot) ||
                !m.number(*ms, "poll_ms", 1, 255, d.mousePollMs) ||
                !m.number(*ms, "queue_depth", 1, 255, d.mouseQueueDepth) ||
                !m.fullPolicy(*ms, d.mouseOnFull) ||
                !m.text(*ms, "name", d.mouseName, false) ||
                !m.number(*ms, "bcd_hid", 1, 0xFFFF, d.mouseBcdHid) ||
                !m.number(*ms, "endpoint_address", 0x81, 0x8F, d.mouseEndpointAddress) ||
                !m.number(*ms, "max_packet", 1, 1024, d.mouseMaxPacket) ||
                !m.number(*ms, "axis_minimum", -32767, 32767, d.mouseAxisMinimum) ||
                !m.number(*ms, "axis_maximum", -32767, 32767, d.mouseAxisMaximum) ||
                !m.number(*ms, "axis_bits", 1, 16, d.mouseAxisBits) ||
                !m.number(*ms, "wheel_minimum", -32767, 32767, d.mouseWheelMinimum) ||
                !m.number(*ms, "wheel_maximum", -32767, 32767, d.mouseWheelMaximum) ||
                !m.number(*ms, "wheel_bits", 1, 16, d.mouseWheelBits))
                return false;
        }
    }
    return true;
}

} // namespace

bool parseMimUsbConfig(const std::string& json, MimUsbConfig& out, std::string& error)
{
    Value root;
    if (!Parser(json).parse(root, error)) return false;
    if (root.kind != Value::Kind::Object) {
        error = "the top level must be an object";
        return false;
    }

    out = MimUsbConfig{};
    {
        Needs top("", error);
        if (!top.boolean(root, "enabled", out.enabled)) return false;
        if (!out.enabled) return true; // off: nothing else is read
        if (!top.number(root, "discover_timeout_ms", 1, 60000, out.discoverTimeoutMs)) return false;
    }

    const Value* devices = member(root, "devices");
    if (devices) {
        if (devices->kind != Value::Kind::Array) {
            error = "\"devices\" must be a list";
            return false;
        }
        size_t index = 0;
        for (const Value& entry : devices->arr) {
            if (entry.kind != Value::Kind::Object) {
                error = "each entry of \"devices\" must be an object";
                return false;
            }
            MimUsbDevice d;
            if (!readDevice(entry, "devices[" + std::to_string(index++) + "].", d, error)) return false;
            out.devices.push_back(d);
        }
    } else {
        // One board, described at the top level.
        MimUsbDevice d;
        if (!readDevice(root, "", d, error)) return false;
        out.devices.push_back(d);
    }
    return true;
}

std::string mimUsbConfigPath()
{
    if (const char* env = std::getenv("MW_MIMUSB_CONFIG"); env && *env) return env;

    std::string base;
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata)
        base = std::string(appdata) + "\\MoonlightWeb\\MoonlightWeb\\";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home && *home)
        base = std::string(home) + "/Library/Application Support/MoonlightWeb/MoonlightWeb/";
#else
    if (const char* home = std::getenv("HOME"); home && *home)
        base = std::string(home) + "/.local/share/MoonlightWeb/MoonlightWeb/";
#endif
    return base.empty() ? std::string() : base + "mimusb.json";
}

MimUsbConfig loadMimUsbConfig(std::string& error)
{
    MimUsbConfig config;
    if (const char* off = std::getenv("MW_MIMUSB_DISABLE"); off && *off && *off != '0')
        return config;

    const std::string path = mimUsbConfigPath();
    if (path.empty()) return config;

    std::ifstream file(path, std::ios::binary);
    if (!file) return config; // no file: the add-on is simply off

    std::ostringstream text;
    text << file.rdbuf();
    if (!parseMimUsbConfig(text.str(), config, error)) {
        error = path + ": " + error;
        config = MimUsbConfig{};
    }
    return config;
}

} // namespace mw::native::input

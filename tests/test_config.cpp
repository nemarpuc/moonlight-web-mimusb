// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
// Config parser test: builds with any C++17 compiler, no framework.
//   c++ -std=c++17 -Ioverlay/backend/native-host/src/input tests/test_config.cpp
//       overlay/backend/native-host/src/input/MimHidConfig.cpp && ./a.out  (one command line)
#include "MimHidConfig.h"
#include "MimHidKeymap.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace mw::native::input;

static int g_failed = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond);                       \
            ++g_failed;                                                               \
        }                                                                             \
    } while (0)

static MimHidConfig parse(const std::string& json, bool expectOk = true)
{
    MimHidConfig c;
    std::string error;
    const bool ok = parseMimHidConfig(json, c, error);
    CHECK(ok == expectOk);
    return c;
}

// The shipped example must always parse (run from the repository root).
static void checkExampleFile()
{
    std::ifstream in("mimhid.example.json");
    CHECK(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    MimHidConfig c = parse(ss.str());
    CHECK(c.enabled);
    CHECK(c.devices.size() == 1);
    CHECK(c.devices[0].keyboardEnabled && c.devices[0].mouseEnabled && c.devices[0].baud == 0);
}

// A whole device description, written out: nothing in the parser has a default, so every test
// builds on this and removes or changes one thing.
static const char* const kLink = R"("link": {
    "handshake_timeout_ms": 50, "control_timeout_ms": 500, "reconnect_timeout_ms": 3000,
    "send_timeout_ms": 100, "drain_timeout_ms": 1000, "ready_timeout_ms": 5000, "max_report_bytes": 64,
    "linux_hid": {"descriptor_bytes": 4096, "fields_per_report": 256, "global_stack_depth": 4,
                  "report_size_bits": 256, "usages": 12288, "report_data_bits": 65528}})";
static const char* const kUsb = R"("usb": {
    "vid": "0x1209", "pid": 2, "manufacturer": "M", "product": "Pad", "serial": "",
    "bcd_device": "0x0100", "bcd_usb": "0x0200", "language_id": "0x0409",
    "ep0_max_packet_size": 64, "configuration_value": 1, "remote_wakeup": true,
    "self_powered": false, "max_power_ma": 200})";
static const char* const kKeyboard = R"("keyboard": {
    "layout": "jis", "boot": false, "poll_ms": 1, "name": "", "country": 15, "bcd_hid": "0x0111",
    "endpoint_address": "0x81", "max_packet": 64, "usage_minimum": 4, "usage_maximum": 221})";
static const char* const kMouse = R"("mouse": {
    "buttons": 3, "wheel": true, "pan": false, "boot": false, "poll_ms": 1, "name": "",
    "bcd_hid": "0x0111", "endpoint_address": "0x82", "max_packet": 64,
    "axis_minimum": -32767, "axis_maximum": 32767, "axis_bits": 16,
    "wheel_minimum": -127, "wheel_maximum": 127, "wheel_bits": 8})";

static std::string device(const std::string& port, const std::string& classes)
{
    return "\"port\": \"" + port + "\", \"baud\": 921600, " + kLink + ", " + kUsb + ", " + classes;
}
static std::string whole(const std::string& port, const std::string& classes)
{
    return "{\"enabled\": true, \"discover_timeout_ms\": 50, " + device(port, classes) + "}";
}

/// The error text of a file that must be refused.
static std::string refused(const std::string& json)
{
    MimHidConfig c;
    std::string error;
    CHECK(!parseMimHidConfig(json, c, error));
    return error;
}

static bool contains(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

int main()
{
    checkExampleFile();
    // One board at the top level, both classes.
    {
        MimHidConfig c = parse(whole("COM5", std::string(kKeyboard) + ", " + kMouse));
        CHECK(c.enabled && c.discoverTimeoutMs == 50);
        CHECK(c.devices.size() == 1);
        const MimHidDevice& d = c.devices[0];
        CHECK(d.port == "COM5" && d.baud == 921600);
        CHECK(d.handshakeTimeoutMs == 50 && d.controlTimeoutMs == 500 && d.reconnectTimeoutMs == 3000);
        CHECK(d.sendTimeoutMs == 100);
        CHECK(d.linuxDescriptorBytes == 4096 && d.linuxReportDataBits == 65528);
        CHECK(d.vendorId == 0x1209 && d.productId == 2);
        CHECK(d.product == "Pad" && d.bcdUsb == 0x0200 && d.languageId == 0x0409);
        CHECK(d.maxPowerMa == 200);
        CHECK(d.keyboardEnabled && d.keyboardJis && d.keyboardCountry == 15);
        CHECK(d.keyboardEndpointAddress == 0x81 && d.keyboardUsageMaximum == 221);
        CHECK(d.mouseEnabled && d.mouseButtons == 3 && !d.mousePan && d.mouseWheel);
        CHECK(d.mouseAxisMinimum == -32767 && d.mouseWheelBits == 8 && d.mouseEndpointAddress == 0x82);
        CHECK(c.active());
    }

    // Two boards: keyboard on one, mouse on the other.
    {
        MimHidConfig c = parse("{\"enabled\": true, \"discover_timeout_ms\": 50, \"devices\": [{" +
                               device("auto", kKeyboard).replace(device("auto", kKeyboard).find("921600"), 6, "0") +
                               "}, {" + device("/dev/ttyACM1", std::string("\"mouse\": {\"enabled\": true, ") +
                                                                 (std::string(kMouse).substr(std::string("\"mouse\": {").size()))) +
                               "}]}");
        CHECK(c.devices.size() == 2);
        CHECK(c.devices[0].keyboardEnabled && !c.devices[0].mouseEnabled && c.devices[0].baud == 0);
        CHECK(!c.devices[1].keyboardEnabled && c.devices[1].mouseEnabled);
        CHECK(c.devices[1].port == "/dev/ttyACM1");
    }

    // A class that is not mentioned is not presented; "enabled": false switches one off.
    {
        MimHidConfig c = parse(whole("COM5", R"("keyboard": {"enabled": false})"));
        CHECK(!c.devices[0].keyboardEnabled && !c.devices[0].mouseEnabled);
        CHECK(!c.active());
    }

    // Off unless asked for, and then nothing else is read.
    {
        MimHidConfig c = parse(R"({"enabled": false, "keyboard": {}})");
        CHECK(!c.enabled && !c.active());
    }
    refused(R"({"keyboard": {}})"); // "enabled" itself is required

    // Nothing has a default: a missing key is refused, and the error names it.
    CHECK(contains(refused(R"({"enabled": true})"), "discover_timeout_ms"));
    CHECK(contains(refused(R"({"enabled": true, "discover_timeout_ms": 50})"), "port"));
    CHECK(contains(refused(R"({"enabled": true, "discover_timeout_ms": 50, "port": "COM5", "baud": 9600})"),
                   "link"));
    {
        // Remove each required piece in turn.
        const std::string good = whole("COM5", std::string(kKeyboard) + ", " + kMouse);
        for (const char* key : {"\"baud\"", "\"handshake_timeout_ms\"", "\"control_timeout_ms\"",
                                "\"reconnect_timeout_ms\"", "\"send_timeout_ms\"", "\"drain_timeout_ms\"", "\"ready_timeout_ms\"",
                                "\"max_report_bytes\"", "\"descriptor_bytes\"", "\"usages\"", "\"vid\"",
                                "\"bcd_usb\"", "\"language_id\"", "\"ep0_max_packet_size\"",
                                "\"configuration_value\"", "\"remote_wakeup\"", "\"max_power_ma\"",
                                "\"serial\"", "\"country\"", "\"bcd_hid\"", "\"endpoint_address\"",
                                "\"max_packet\"", "\"usage_maximum\"", "\"axis_bits\"", "\"wheel_minimum\"",
                                "\"buttons\"", "\"layout\""}) {
            std::string broken = good;
            const size_t at = broken.find(key);
            CHECK(at != std::string::npos);
            if (at == std::string::npos) continue;
            broken.replace(at, std::string(key).size(), "\"renamed\"");
            const std::string why = refused(broken);
            CHECK(contains(why, "missing key"));
            CHECK(contains(why, std::string(key).substr(1, std::string(key).size() - 2).c_str()));
        }
    }

    // Out-of-range values are refused, not clamped; a zero baud needs the port "auto"; the vendor
    // ID is never zero; unknown keys are ignored.
    {
        std::string bad = whole("COM5", kKeyboard);
        bad.replace(bad.find("\"poll_ms\": 1"), 12, "\"poll_ms\": 999");
        CHECK(contains(refused(bad), "keyboard.poll_ms"));
        std::string zero = whole("COM5", kKeyboard);
        zero.replace(zero.find("921600"), 6, "0");
        CHECK(contains(refused(zero), "baud"));
        std::string novid = whole("COM5", kKeyboard);
        novid.replace(novid.find("\"vid\": \"0x1209\""), 15, "\"vid\": 0");
        CHECK(contains(refused(novid), "usb.vid"));
        std::string extra = whole("COM5", kKeyboard);
        extra.insert(extra.size() - 1, ", \"future_key\": [1, {\"a\": 2}]");
        parse(extra);
    }

    // Bad input is refused with a reason, not guessed at.
    parse("{", false);
    parse("[]", false);
    parse(R"({"enabled": true, "discover_timeout_ms": 50, "devices": {}})", false);
    parse(R"({"enabled": true, "discover_timeout_ms": 50, "devices": [1]})", false);

    // Key map: positions, not characters.
    CHECK(vkToHidUsage('A', false) == 0x04);
    CHECK(vkToHidUsage('0', false) == 0x27);
    CHECK(vkToHidUsage(0x0D, false) == 0x28);
    CHECK(vkToHidUsage(0xDC, false) == 0x31);
    CHECK(vkToHidUsage(0xDC, true) == 0x89);
    CHECK(vkToHidUsage(0xA2, false) == 0xE0);
    CHECK(vkToHidUsage(0xFF, false) == 0);
    uint16_t u = 0;
    bool shift = false;
    CHECK(asciiToHid('a', u, shift) && u == 0x04 && !shift);
    CHECK(asciiToHid('A', u, shift) && u == 0x04 && shift);
    CHECK(asciiToHid('!', u, shift) && u == 0x1E && shift);
    CHECK(asciiToHid(')', u, shift) && u == 0x27 && shift);
    CHECK(!asciiToHid('\x01', u, shift));

    if (g_failed == 0) std::printf("all passed\n");
    return g_failed == 0 ? 0 : 1;
}

/*
 * MoonlightWeb physical-HID add-on (unofficial).
 * Copyright (C) 2026 the moonlight-web-mimhid contributors
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

#include "MimHidInput.h"

#include "../core/Log.h"
#include "MimHidKeymap.h"

#include <mimhid.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <set>
#include <vector>

namespace mw::native::input {

// ── One board ───────────────────────────────────────────────────────────────
//
// Opening a board re-enumerates its USB device on the target, which sees a
// plug-in: boards are opened once per process (see Registry) so that a keyboard
// does not vanish and reappear at every stream.
class MimHidBoard
{
public:
    ~MimHidBoard()
    {
        if (m_Device) mimhid_device_close(m_Device, m_ControlTimeoutMs, m_DrainTimeoutMs);
    }

    static std::shared_ptr<MimHidBoard> open(const MimHidDevice& d, const std::string& port,
                                             uint32_t baud, std::string& error)
    {
        auto board = std::shared_ptr<MimHidBoard>(new MimHidBoard());
        if (!board->openDevice(d, port, baud, error)) return nullptr;
        return board;
    }

    const std::string& port() const { return m_Port; }
    bool hasKeyboard() const { return m_Keyboard != nullptr; }
    bool hasMouse() const { return m_Mouse != nullptr; }
    /// Three failures in a row: the board was unplugged or stopped answering.
    bool broken() const { return m_Consecutive.load(std::memory_order_relaxed) >= 3; }

    void key(uint16_t usage, bool down)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Keyboard) return;
        note(mimhid_keyboard_key(m_Keyboard, usage, down ? 1 : 0));
        note(mimhid_node_submit(m_Keyboard, m_SendTimeoutMs));
    }
    void move(int dx, int dy)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Mouse) return;
        note(mimhid_mouse_move(m_Mouse, dx, dy));
        note(mimhid_node_submit(m_Mouse, m_SendTimeoutMs));
    }
    void button(unsigned hidButton, bool down)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Mouse) return;
        note(mimhid_mouse_button(m_Mouse, hidButton, down ? 1 : 0));
        note(mimhid_node_submit(m_Mouse, m_SendTimeoutMs));
    }
    void scroll(int wheel, int pan)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Mouse) return;
        note(mimhid_mouse_scroll(m_Mouse, wheel, pan));
        note(mimhid_node_submit(m_Mouse, m_SendTimeoutMs));
    }

private:
    MimHidBoard() = default;

    /// Logs the first few failures only: a board unplugged mid-stream would
    /// otherwise write a line per mouse movement.
    void note(mimhid_result result)
    {
        if (result == MIMHID_OK) {
            m_Consecutive.store(0, std::memory_order_relaxed);
            return;
        }
        m_Consecutive.fetch_add(1, std::memory_order_relaxed);
        const int n = m_Failures.fetch_add(1, std::memory_order_relaxed);
        if (n < 5)
            log::warning("[mimhid] " + m_Port + ": " + mimhid_result_name(result) + " (" +
                         mimhid_last_error()->reason + ")");
    }

    bool fail(const std::string& what, mimhid_result r, std::string& error)
    {
        error = what + ": " + mimhid_result_name(r) + " (" + mimhid_last_error()->reason + ")";
        return false;
    }

    bool openDevice(const MimHidDevice& d, const std::string& port, uint32_t baud, std::string& error)
    {
        m_Port = port;
        m_Manufacturer = d.manufacturer;
        m_Product = d.product;
        m_Serial = d.serial;
        m_KeyboardName = d.keyboardName;
        m_MouseName = d.mouseName;
        m_ControlTimeoutMs = d.controlTimeoutMs;
        m_DrainTimeoutMs = d.drainTimeoutMs;
        m_SendTimeoutMs = d.sendTimeoutMs;

        // Every value comes from the configuration (libmimhid fills nothing in).
        if (baud == 0) {
            error = "no baud rate for " + port + " (a named port needs one in the configuration)";
            return false;
        }
        mimhid_device_config config;
        mimhid_device_config_init(&config);
        config.transport.baud_rate = baud;
        config.transport.handshake_timeout_ms = d.handshakeTimeoutMs;
        config.transport.maximum_report_bytes = d.maxReportBytes;
        config.transport.linux_hid.descriptor_bytes = d.linuxDescriptorBytes;
        config.transport.linux_hid.fields_per_report = d.linuxFieldsPerReport;
        config.transport.linux_hid.global_stack_depth = d.linuxGlobalStackDepth;
        config.transport.linux_hid.report_size_bits = d.linuxReportSizeBits;
        config.transport.linux_hid.usages = d.linuxUsages;
        config.transport.linux_hid.report_data_bits = d.linuxReportDataBits;
        config.transport.log_level = MIMHID_LOG_DISABLED;
        config.usb_device.bcd_usb = d.bcdUsb;
        config.usb_device.language_id = d.languageId;
        config.usb_device.ep0_max_packet_size = static_cast<uint8_t>(d.ep0MaxPacketSize);
        config.usb_config.configuration_value = static_cast<uint8_t>(d.configurationValue);
        config.usb_device.vendor_id = d.vendorId;
        config.usb_device.product_id = d.productId;
        config.usb_device.manufacturer = m_Manufacturer.c_str();
        config.usb_device.product = m_Product.c_str();
        config.usb_device.bcd_device = d.bcdDevice;
        if (!m_Serial.empty()) {
            config.usb_device.serial = m_Serial.c_str();
            config.usb_device.serial_from_chip_id = 0;
        } else {
            config.usb_device.serial_from_chip_id = 1; // an empty serial in the file: the chip's id
        }
        // bit 7 is always set; bit 6 self-powered; bit 5 remote wakeup.
        config.usb_config.bm_attributes =
            static_cast<uint8_t>(0x80 | (d.selfPowered ? 0x40 : 0) | (d.remoteWakeup ? 0x20 : 0));
        config.usb_config.bmax_power = static_cast<uint8_t>(d.maxPowerMa / 2);
        // Closing the board sends every key up and stops talking to it, but leaves
        // the USB device on the target: it does not vanish between streams.
        config.transport.keep_attached = 1;

        mimhid_result r = mimhid_device_open(port.c_str(), &config, &m_Device);
        if (r != MIMHID_OK) return fail("open " + port, r, error);

        mimhid_commit_timeouts timeouts;
        mimhid_commit_timeouts_init(&timeouts);
        timeouts.control_timeout_ms = d.controlTimeoutMs;
        timeouts.reconnect_timeout_ms = d.reconnectTimeoutMs;

        // Start from "unplugged". A previous process may have left the board
        // attached (it was killed, or closed with keep_attached); configuring on
        // top of that, the target does not mount the new device in time (6 of 6
        // opens timed out on the test bench; with this, 6 of 6 take 0.4 s).
        r = mimhid_device_disconnect(m_Device, d.controlTimeoutMs);
        if (r != MIMHID_OK) return fail("reset " + port, r, error);

        // Only the classes asked for get an interface: the target sees a
        // keyboard, a mouse, or both, and nothing else.
        mimhid_spec* keyboardSpec = nullptr;
        mimhid_spec* mouseSpec = nullptr;
        if (d.keyboardEnabled) {
            mimhid_keyboard_options options;
            mimhid_keyboard_options_init(&options);
            options.usage_minimum = static_cast<uint16_t>(d.keyboardUsageMinimum);
            options.usage_maximum = static_cast<uint16_t>(d.keyboardUsageMaximum);
            r = mimhid_spec_create_keyboard(&options, &keyboardSpec);
            if (r != MIMHID_OK) return fail("keyboard descriptor", r, error);
        }
        if (d.mouseEnabled) {
            mimhid_mouse_options options;
            mimhid_mouse_options_init(&options);
            options.button_count = d.mouseButtons;
            options.x = mimhid_integer_field_make(d.mouseAxisMinimum, d.mouseAxisMaximum, d.mouseAxisBits);
            options.y = options.x;
            options.enable_wheel = d.mouseWheel ? 1 : 0;
            options.wheel = mimhid_integer_field_make(d.mouseWheelMinimum, d.mouseWheelMaximum,
                                                      d.mouseWheelBits);
            options.enable_pan = d.mousePan ? 1 : 0;
            options.pan = options.wheel;
            r = mimhid_spec_create_mouse(&options, &mouseSpec);
            if (r != MIMHID_OK) {
                if (keyboardSpec) mimhid_spec_release(keyboardSpec);
                return fail("mouse descriptor", r, error);
            }
        }

        // One batch: the board re-enumerates once, not once per interface.
        uint8_t iface = 0;
        mimhid_device_batch_begin(m_Device);
        // A HID interface (HID 1.11 4.1: class 0x03); every other value is the configuration's.
        if (keyboardSpec) {
            mimhid_node_config node;
            mimhid_node_config_init(&node);
            node.iface.number = iface++;
            node.iface.class_code = 0x03;
            node.iface.bcd_hid = d.keyboardBcdHid;
            node.ep_in.address = static_cast<uint8_t>(d.keyboardEndpointAddress);
            node.ep_in.max_packet_size = static_cast<uint16_t>(d.keyboardMaxPacket);
            node.ep_in.poll_interval_ms = static_cast<uint8_t>(d.keyboardPollMs);
            if (d.keyboardBoot) {
                node.iface.subclass = 1;
                node.iface.protocol = 1;
            }
            node.iface.country_code = static_cast<uint8_t>(d.keyboardCountry);
            if (!m_KeyboardName.empty()) node.iface.name = m_KeyboardName.c_str();
            r = mimhid_node_open(m_Device, keyboardSpec, &node, &timeouts, &m_Keyboard);
            mimhid_spec_release(keyboardSpec);
            if (r != MIMHID_OK) {
                if (mouseSpec) mimhid_spec_release(mouseSpec);
                return fail("open keyboard", r, error);
            }
        }
        if (mouseSpec) {
            mimhid_node_config node;
            mimhid_node_config_init(&node);
            node.iface.number = iface++;
            node.iface.class_code = 0x03;
            node.iface.bcd_hid = d.mouseBcdHid;
            node.ep_in.address = static_cast<uint8_t>(d.mouseEndpointAddress);
            node.ep_in.max_packet_size = static_cast<uint16_t>(d.mouseMaxPacket);
            node.ep_in.poll_interval_ms = static_cast<uint8_t>(d.mousePollMs);
            if (d.mouseBoot) {
                node.iface.subclass = 1;
                node.iface.protocol = 2;
            }
            if (!m_MouseName.empty()) node.iface.name = m_MouseName.c_str();
            r = mimhid_node_open(m_Device, mouseSpec, &node, &timeouts, &m_Mouse);
            mimhid_spec_release(mouseSpec);
            if (r != MIMHID_OK) return fail("open mouse", r, error);
        }
        r = mimhid_device_batch_end(m_Device, &timeouts);
        if (r != MIMHID_OK) return fail("enumerate", r, error);
        r = mimhid_device_wait_ready(m_Device, d.readyTimeoutMs);
        if (r != MIMHID_OK) return fail("wait for the board", r, error);

        log::info("[mimhid] board ready on " + port + (m_Keyboard ? ", keyboard" : "") +
                  (m_Mouse ? ", mouse" : ""));
        return true;
    }

    std::mutex m_Mutex;
    mimhid_device* m_Device = nullptr;
    mimhid_node* m_Keyboard = nullptr;
    mimhid_node* m_Mouse = nullptr;
    std::string m_Port;
    std::string m_Manufacturer;
    std::string m_Product;
    std::string m_Serial;
    std::string m_KeyboardName;
    std::string m_MouseName;
    uint32_t m_ControlTimeoutMs = 0;
    uint32_t m_DrainTimeoutMs = 0;
    uint32_t m_SendTimeoutMs = 0;
    std::atomic<int> m_Failures{0};
    std::atomic<int> m_Consecutive{0};
};

// ── Every board of the process ──────────────────────────────────────────────

namespace {

/// What a configuration resolved to: who carries the keyboard, who the mouse.
struct Routing
{
    std::shared_ptr<MimHidBoard> keyboard;
    std::shared_ptr<MimHidBoard> mouse;
    bool jis = false;
    uint32_t mouseButtons = 5;
};

class Registry
{
public:
    /// The first call opens what it can, at once, and starts the background
    /// retry; later calls return the same registry (the configuration of the
    /// first one stays for the life of the process).
    static Registry& get(const MimHidConfig& config)
    {
        static std::atomic<Registry*> instance{nullptr};
        static std::mutex mutex;
        if (Registry* r = instance.load(std::memory_order_acquire)) return *r;
        std::lock_guard<std::mutex> lock(mutex);
        Registry* r = instance.load(std::memory_order_acquire);
        if (!r) {
            r = new Registry(config); // lives to the end of the process
            std::thread([r] { r->loop(); }).detach();
            instance.store(r, std::memory_order_release);
        }
        return *r;
    }

    /// A sink starting. Never waits for a board: the first one only wakes the
    /// background thread, and until a board answers the OS sink carries the input.
    void addUser()
    {
        if (m_Users.fetch_add(1) != 0) return;
        {
            std::lock_guard<std::mutex> lock(m_WakeMutex);
            m_Poke = true;
        }
        m_Wake.notify_one();
    }

    /// A sink stopped: the last one closes the boards (every key up, serial port
    /// released; the USB device stays on the target). Sinks must have let go of
    /// their board pointers first, or the board stays open until they do.
    void removeUser()
    {
        std::lock_guard<std::mutex> lock(m_SlotMutex);
        if (m_Users.load() > 0 && m_Users.fetch_sub(1) == 1) {
            for (Slot& s : m_Slots) {
                s.board.reset();
                s.lastReport.clear();
            }
            publish();
        }
    }

    Routing routing()
    {
        std::lock_guard<std::mutex> lock(m_RouteMutex);
        return m_Routing;
    }
    uint64_t generation() const { return m_Generation.load(std::memory_order_acquire); }

private:
    struct Slot
    {
        MimHidDevice cfg;
        std::shared_ptr<MimHidBoard> board;
        std::string lastReport; ///< the last problem logged, so a standing one is logged once
    };

    explicit Registry(const MimHidConfig& config) : m_DiscoverTimeoutMs(config.discoverTimeoutMs)
    {
        for (const MimHidDevice& d : config.devices)
            if (d.carriesAnything()) m_Slots.push_back({d, nullptr, {}});
    }

    /// A board that cannot be had is retried every 5 seconds: a cable pulled and
    /// pushed back, a program that held the port, the previous stream's process
    /// still closing it — all the same.
    void loop()
    {
        for (;;) {
            {
                // Every 5 seconds, or at once when a sink starts.
                std::unique_lock<std::mutex> wake(m_WakeMutex);
                m_Wake.wait_for(wake, std::chrono::seconds(5), [this] { return m_Poke; });
                m_Poke = false;
            }
            std::lock_guard<std::mutex> lock(m_SlotMutex);
            if (m_Users.load() > 0) tick(); // idle between streams: nothing to keep open
        }
    }

    void report(Slot& s, const std::string& what)
    {
        if (what == s.lastReport) return;
        s.lastReport = what;
        log::warning("[mimhid] " + what + "; its classes stay on the OS, trying again every 5 s");
    }

    /// Ports discovery finds that no entry names and no open board holds. "auto"
    /// entries take them in order, so two of them never get the same board.
    static std::vector<std::pair<std::string, uint32_t>> sparePorts(const std::set<std::string>& excluded,
                                                                     uint32_t discoverTimeoutMs)
    {
        std::vector<std::pair<std::string, uint32_t>> spare;
        mimhid_discovery* found = nullptr;
        if (mimhid_discover(discoverTimeoutMs, &found) == MIMHID_OK && found) {
            const size_t n = mimhid_discovery_count(found);
            for (size_t i = 0; i < n; ++i) {
                const mimhid_device_info* info = mimhid_discovery_get(found, i);
                if (info && !excluded.count(info->port_name))
                    spare.emplace_back(info->port_name, info->baud_rate);
            }
        }
        if (found) mimhid_discovery_destroy(found);
        return spare;
    }

    void tick()
    {
        // 1. a board that stopped answering is let go (closing it)
        for (Slot& s : m_Slots) {
            if (s.board && s.board->broken()) {
                log::warning("[mimhid] " + s.board->port() + " stopped answering; reconnecting");
                s.board.reset();
                s.lastReport.clear();
            }
        }

        // 2. every slot without a board tries to get one
        std::set<std::string> excluded;
        bool needAuto = false;
        for (const Slot& s : m_Slots) {
            if (s.board) excluded.insert(s.board->port());
            if (s.cfg.port != "auto") excluded.insert(s.cfg.port);
            else if (!s.board) needAuto = true;
        }
        std::vector<std::pair<std::string, uint32_t>> spare;
        if (needAuto) spare = sparePorts(excluded, m_DiscoverTimeoutMs);

        for (Slot& s : m_Slots) {
            if (s.board) continue;
            std::string port = s.cfg.port;
            uint32_t baud = s.cfg.baud;
            if (port == "auto") {
                if (spare.empty()) {
                    report(s, "no board found for an \"auto\" entry");
                    continue;
                }
                port = spare.front().first;
                if (baud == 0) baud = spare.front().second;
                spare.erase(spare.begin());
            }
            std::string error;
            std::shared_ptr<MimHidBoard> board = MimHidBoard::open(s.cfg, port, baud, error);
            if (!board) {
                report(s, port + " unavailable: " + error);
                continue;
            }
            s.board = board;
            s.lastReport.clear();
        }

        publish();
    }

    /// Who carries what, now.
    void publish()
    {
        Routing next;
        for (const Slot& s : m_Slots) {
            if (!s.board) continue;
            if (!next.keyboard && s.cfg.keyboardEnabled && s.board->hasKeyboard()) {
                next.keyboard = s.board;
                next.jis = s.cfg.keyboardJis;
            }
            if (!next.mouse && s.cfg.mouseEnabled && s.board->hasMouse()) {
                next.mouse = s.board;
                next.mouseButtons = s.cfg.mouseButtons;
            }
        }
        std::lock_guard<std::mutex> lock(m_RouteMutex);
        if (next.keyboard != m_Routing.keyboard || next.mouse != m_Routing.mouse) {
            m_Routing = next;
            m_Generation.fetch_add(1, std::memory_order_release);
        }
    }

    const uint32_t m_DiscoverTimeoutMs;
    std::vector<Slot> m_Slots; // under m_SlotMutex
    std::atomic<int> m_Users{0}; // sinks between start() and stop()
    std::mutex m_SlotMutex;
    std::mutex m_WakeMutex;
    std::condition_variable m_Wake;
    bool m_Poke = false;
    std::mutex m_RouteMutex;
    Routing m_Routing;
    std::atomic<uint64_t> m_Generation{0};
};

bool isKeyboardEvent(InputEvent::Type t)
{
    switch (t) {
    case InputEvent::Type::KeyDown:
    case InputEvent::Type::KeyUp:
    case InputEvent::Type::Utf8Text:
    case InputEvent::Type::CharDown:
    case InputEvent::Type::CharUp:
    case InputEvent::Type::LockKeySync: return true;
    default: return false;
    }
}

bool isMouseEvent(InputEvent::Type t)
{
    switch (t) {
    case InputEvent::Type::MouseMoveRelative:
    case InputEvent::Type::MouseMoveAbsolute:
    case InputEvent::Type::MouseButtonDown:
    case InputEvent::Type::MouseButtonUp:
    case InputEvent::Type::MouseScrollVertical:
    case InputEvent::Type::MouseScrollHorizontal: return true;
    default: return false;
    }
}

/// The browser numbers buttons 1 left, 2 middle, 3 right, 4 X1, 5 X2; HID
/// numbers them 1 left, 2 right, 3 middle, 4 back, 5 forward.
unsigned toHidButton(int browserButton)
{
    switch (browserButton) {
    case 1: return 1;
    case 2: return 3;
    case 3: return 2;
    case 4: return 4;
    case 5: return 5;
    default: return 0;
    }
}

constexpr uint16_t kLeftShift = 0xE1;

} // namespace

// ── The sink ────────────────────────────────────────────────────────────────

MimHidInput::MimHidInput(std::unique_ptr<IInputSink> osSink, const MimHidConfig& config)
    : m_Os(std::move(osSink)), m_Config(config)
{
}

MimHidInput::~MimHidInput()
{
    stop();
}

bool MimHidInput::start(std::string& error)
{
    if (!m_Os->start(error)) return false;

    Registry::get(m_Config).addUser();
    m_Registered = true;
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Generation = ~uint64_t(0); // look at the registry on the first event
    refreshRouting();
    if (!m_Keyboard && !m_Mouse)
        log::warning("[mimhid] no board is carrying input yet; using the OS input path until one answers");
    return true;
}

void MimHidInput::refreshRouting()
{
    Registry& registry = Registry::get(m_Config);
    const uint64_t generation = registry.generation();
    if (generation == m_Generation) return;
    m_Generation = generation;
    const Routing now = registry.routing();
    // A board that went away took its held keys with it; the ones on the new
    // board start from nothing.
    if (now.keyboard != m_Keyboard) {
        m_KeysDown.reset();
        m_CharShifted.reset();
        m_Keyboard = now.keyboard;
    }
    if (now.mouse != m_Mouse) {
        m_ButtonsDown = 0;
        m_WheelRemainder = 0;
        m_PanRemainder = 0;
        m_HaveAbsolute = false;
        m_Mouse = now.mouse;
    }
    m_Jis = now.jis;
    m_MouseButtonCount = now.mouseButtons;
}

void MimHidInput::stop()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    releaseEverything();
    m_Keyboard.reset();
    m_Mouse.reset();
    const bool registered = m_Registered;
    m_Registered = false;
    if (registered) Registry::get(m_Config).removeUser(); // the last one closes the boards
    if (m_Os) m_Os->stop();
}

// A physical keyboard has no "session ended mid-keypress"; a viewer's does.
void MimHidInput::releaseEverything()
{
    if (m_Keyboard) {
        for (size_t usage = 0; usage < m_KeysDown.size(); ++usage)
            if (m_KeysDown.test(usage)) m_Keyboard->key(static_cast<uint16_t>(usage), false);
    }
    m_KeysDown.reset();
    m_CharShifted.reset();
    if (m_Mouse) {
        for (unsigned b = 1; b <= 5; ++b)
            if (m_ButtonsDown & (1u << (b - 1))) m_Mouse->button(b, false);
    }
    m_ButtonsDown = 0;
}

void MimHidInput::pressUsage(uint16_t usage, bool down, bool resync)
{
    if (usage == 0 || usage >= m_KeysDown.size()) return;
    const bool held = m_KeysDown.test(usage);
    // The client's heartbeat re-presses what it believes is held: applied
    // blindly that is an extra character. Only a real change goes out.
    if (resync && down == held) return;
    if (down == held && !down) return;
    m_KeysDown.set(usage, down);
    m_Keyboard->key(usage, down);
}

void MimHidInput::injectKey(const InputEvent& event, bool down)
{
    pressUsage(vkToHidUsage(event.keyCode, m_Jis), down, event.resync);
}

// The viewer's layout produced a character; the target's layout is unknown to
// us, so the character is typed as a US board would — right for the usual
// case of a US target, honest about being a guess otherwise.
void MimHidInput::injectChar(const InputEvent& event, bool down)
{
    uint16_t usage = 0;
    bool shift = false;
    if (event.text.size() == 1 && asciiToHid(event.text[0], usage, shift) && !event.chord) {
        if (down) {
            if (shift && !m_KeysDown.test(kLeftShift)) {
                m_CharShifted.set(usage);
                pressUsage(kLeftShift, true, false);
            }
            pressUsage(usage, true, event.resync);
        } else {
            pressUsage(usage, false, event.resync);
            if (m_CharShifted.test(usage)) {
                m_CharShifted.reset(usage);
                pressUsage(kLeftShift, false, false);
            }
        }
        return;
    }
    pressUsage(vkToHidUsage(event.keyCode, m_Jis), down, event.resync);
}

void MimHidInput::injectText(const std::string& text)
{
    for (const char c : text) {
        uint16_t usage = 0;
        bool shift = false;
        if (!asciiToHid(c, usage, shift)) continue; // no layout knowledge for the rest
        const bool addShift = shift && !m_KeysDown.test(kLeftShift);
        if (addShift) pressUsage(kLeftShift, true, false);
        pressUsage(usage, true, false);
        pressUsage(usage, false, false);
        if (addShift) pressUsage(kLeftShift, false, false);
    }
}

// The board's mouse is relative, as every USB mouse is. An absolute position
// (a touch trackpad, a pointer that is not locked) becomes the movement from
// the previous one; the first only sets the baseline. The target's pointer
// acceleration applies to it, so absolute aiming through the board is
// approximate by nature.
void MimHidInput::injectMouseAbsolute(const InputEvent& event)
{
    if (!m_HaveAbsolute) {
        m_HaveAbsolute = true;
    } else {
        const int dx = event.positionX - m_LastAbsX;
        const int dy = event.positionY - m_LastAbsY;
        if (dx != 0 || dy != 0) m_Mouse->move(dx, dy);
    }
    m_LastAbsX = event.positionX;
    m_LastAbsY = event.positionY;
}

void MimHidInput::injectScroll(int amount, bool horizontal)
{
    int& remainder = horizontal ? m_PanRemainder : m_WheelRemainder;
    remainder += amount;
    const int notches = remainder / 120; // quantised upstream in 120-unit notches
    if (notches == 0) return;
    remainder -= notches * 120;
    if (horizontal)
        m_Mouse->scroll(0, notches);
    else
        m_Mouse->scroll(notches, 0);
}

void MimHidInput::inject(const InputEvent& event)
{
    const bool keyboard = isKeyboardEvent(event.type);
    const bool mouse = isMouseEvent(event.type);

    std::lock_guard<std::mutex> lock(m_Mutex);
    refreshRouting();
    if ((keyboard && routesKeyboard()) || (mouse && routesMouse())) {
        switch (event.type) {
        case InputEvent::Type::KeyDown: injectKey(event, true); break;
        case InputEvent::Type::KeyUp: injectKey(event, false); break;
        case InputEvent::Type::CharDown: injectChar(event, true); break;
        case InputEvent::Type::CharUp: injectChar(event, false); break;
        case InputEvent::Type::Utf8Text: injectText(event.text); break;
        case InputEvent::Type::LockKeySync: break; // the target owns its own lock state
        case InputEvent::Type::MouseMoveRelative:
            if (event.deltaX != 0 || event.deltaY != 0) m_Mouse->move(event.deltaX, event.deltaY);
            break;
        case InputEvent::Type::MouseMoveAbsolute: injectMouseAbsolute(event); break;
        case InputEvent::Type::MouseButtonDown:
        case InputEvent::Type::MouseButtonUp: {
            const unsigned button = toHidButton(event.button);
            if (button == 0 || button > m_MouseButtonCount) break;
            const bool down = event.type == InputEvent::Type::MouseButtonDown;
            const bool held = (m_ButtonsDown & (1u << (button - 1))) != 0;
            if (down == held) break; // a heartbeat must not double-click
            m_ButtonsDown = static_cast<uint8_t>(down ? (m_ButtonsDown | (1u << (button - 1)))
                                                      : (m_ButtonsDown & ~(1u << (button - 1))));
            m_Mouse->button(button, down);
            break;
        }
        case InputEvent::Type::MouseScrollVertical: injectScroll(event.scrollAmount, false); break;
        case InputEvent::Type::MouseScrollHorizontal: injectScroll(event.scrollAmount, true); break;
        default: break;
        }
        return;
    }
    m_Os->inject(event);
}

void MimHidInput::setDisplayRect(int l, int t, int r, int b) { m_Os->setDisplayRect(l, t, r, b); }
void MimHidInput::setDesktopRect(int l, int t, int r, int b) { m_Os->setDesktopRect(l, t, r, b); }

void MimHidInput::setAllowElevated(bool allow)
{
    m_AllowElevated = allow;
    m_Os->setAllowElevated(allow);
}

void MimHidInput::setGateCallback(InputGateCallback callback)
{
    m_Os->setGateCallback(std::move(callback));
}

bool MimHidInput::releaseBlock() { return m_Os->releaseBlock(); }
bool MimHidInput::showsPointer() { return m_Os->showsPointer(); }

std::unique_ptr<IInputSink> wrapWithMimHid(std::unique_ptr<IInputSink> osSink, bool allowElevated)
{
    std::string error;
    const MimHidConfig config = loadMimHidConfig(error);
    if (!error.empty()) log::warning("[mimhid] configuration ignored: " + error);
    if (!config.active()) return osSink;

    auto wrapped = std::make_unique<MimHidInput>(std::move(osSink), config);
    wrapped->setAllowElevated(allowElevated);
    return wrapped;
}

} // namespace mw::native::input

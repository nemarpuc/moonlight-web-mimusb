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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mw::native::input {

/// One libmimhid board on one serial port, and what it presents to its target.
///
/// A board with both classes enabled is one composite USB device with an
/// interface per class. A class left disabled is not presented, and the viewer's
/// events of that class stay on the OS path.
///
/// Nothing here has a default: every value comes from the configuration file, and
/// parseMimHidConfig refuses a file that leaves one out, naming the key. (libmimhid
/// fills nothing in either.)
struct MimHidDevice
{
    std::string port;  ///< "auto", or "COM5" / "/dev/ttyACM0"
    uint32_t baud = 0; ///< a rate in baud, or 0 for "auto" ports: the fastest rate discovery found

    // The link to the board (libmimhid takes every timeout as an argument).
    uint32_t handshakeTimeoutMs = 0;
    uint32_t controlTimeoutMs = 0;
    uint32_t reconnectTimeoutMs = 0;
    uint32_t sendTimeoutMs = 0;  ///< bounds one report's write that makes no progress (it never waits for the target)
    uint32_t drainTimeoutMs = 0; ///< closing: how long the neutral reports may take
    uint32_t readyTimeoutMs = 0; ///< how long the target may take to mount the device
    uint32_t maxReportBytes = 0; ///< the longest report this add-on sends
    // The Linux HID parser limits libmimhid checks a descriptor against (docs/LIMITS.md there).
    uint32_t linuxDescriptorBytes = 0;
    uint32_t linuxFieldsPerReport = 0;
    uint32_t linuxGlobalStackDepth = 0;
    uint32_t linuxReportSizeBits = 0;
    uint32_t linuxUsages = 0;
    uint32_t linuxReportDataBits = 0;

    uint16_t vendorId = 0; ///< A device handed to other people needs IDs of its own; never copy another maker's.
    uint16_t productId = 0;
    std::string manufacturer;
    std::string product;
    std::string serial; ///< empty: the chip's unique id
    uint16_t bcdDevice = 0;
    uint16_t bcdUsb = 0;
    uint16_t languageId = 0;
    uint32_t ep0MaxPacketSize = 0; ///< 8, 16, 32 or 64
    uint32_t configurationValue = 0;
    bool remoteWakeup = false;
    bool selfPowered = false;
    uint32_t maxPowerMa = 0; ///< 0 to 500

    bool keyboardEnabled = false;
    bool keyboardJis = false;    ///< the target uses a JIS layout
    bool keyboardBoot = false;   ///< boot-protocol keyboard (BIOS, bootloaders)
    uint32_t keyboardPollMs = 0; ///< 1 to 255
    std::string keyboardName;    ///< interface string, empty for none
    int keyboardCountry = 0;     ///< HID country code 0-35
    uint16_t keyboardBcdHid = 0;
    uint32_t keyboardEndpointAddress = 0; ///< IN endpoint, 0x81 to 0x8F
    uint32_t keyboardMaxPacket = 0;
    uint32_t keyboardUsageMinimum = 0;
    uint32_t keyboardUsageMaximum = 0;

    bool mouseEnabled = false;
    bool mouseBoot = false; ///< boot-protocol mouse
    uint32_t mouseButtons = 0;
    bool mouseWheel = false;
    bool mousePan = false;
    uint32_t mousePollMs = 0; ///< 1 to 255
    std::string mouseName;
    uint16_t mouseBcdHid = 0;
    uint32_t mouseEndpointAddress = 0; ///< IN endpoint, 0x81 to 0x8F
    uint32_t mouseMaxPacket = 0;
    int mouseAxisMinimum = 0; ///< X and Y
    int mouseAxisMaximum = 0;
    uint32_t mouseAxisBits = 0;
    int mouseWheelMinimum = 0; ///< wheel and pan
    int mouseWheelMaximum = 0;
    uint32_t mouseWheelBits = 0;

    bool carriesAnything() const { return keyboardEnabled || mouseEnabled; }
};

struct MimHidConfig
{
    bool enabled = false;
    uint32_t discoverTimeoutMs = 0; ///< how long a search for boards waits for each port's answer
    std::vector<MimHidDevice> devices;

    /// Whether anything is routed to a board.
    bool active() const
    {
        if (!enabled) return false;
        for (const MimHidDevice& d : devices)
            if (d.carriesAnything()) return true;
        return false;
    }
};

/// Parses @p json into @p out.
///
/// The file is either {"enabled": true, "devices": [ {...}, {...} ]}, or — for
/// the common one-board case — the device's keys beside "enabled" at the top
/// level. Within a device, "keyboard" and "mouse" objects switch each class on
/// ("enabled" defaults to true when the object is present, false when absent).
/// Unknown keys are ignored. With "enabled": false nothing else is read.
/// Otherwise every value is required (no key has a default): returns false, naming
/// the key (for example "devices[0].usb.vid"), when one is missing or out of range,
/// and when the text is not JSON of the expected shape.
bool parseMimHidConfig(const std::string& json, MimHidConfig& out, std::string& error);

/// $MW_MIMHID_CONFIG if set, otherwise mimhid.json beside upstream's
/// settings.json. Empty when no such place can be worked out.
std::string mimHidConfigPath();

/// Loads the file at mimHidConfigPath(). A missing file is not an error: it
/// returns a config with enabled == false, as does MW_MIMHID_DISABLE=1.
MimHidConfig loadMimHidConfig(std::string& error);

} // namespace mw::native::input

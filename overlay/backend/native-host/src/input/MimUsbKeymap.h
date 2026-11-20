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

#pragma once

#include <cstdint>

namespace mw::native::input {

/// Windows virtual-key code (what the browser sends) -> USB HID Keyboard page
/// Usage. HID names a key by its place on the board, so this is a table of
/// positions: the target's own layout decides which character comes out.
/// Returns 0 for a key with no Usage.
///
/// @param jis  the target's layout is JIS. The keys only a JIS board has
///             (Yen, Ro, Henkan, Muhenkan, Hiragana/Katakana) have Usages of
///             their own that are sent in both modes; "jis" additionally puts
///             the Yen key where JIS has it. A full positional remap of the
///             other JIS-specific places (@ [ ] : and so on) is not done.
inline uint16_t vkToHidUsage(int vk, bool jis)
{
    // Letters A-Z
    if (vk >= 0x41 && vk <= 0x5A) return static_cast<uint16_t>(0x04 + (vk - 0x41));
    // Digits 1-9, 0
    if (vk >= 0x31 && vk <= 0x39) return static_cast<uint16_t>(0x1E + (vk - 0x31));
    if (vk == 0x30) return 0x27;
    // F1-F12, F13-F24
    if (vk >= 0x70 && vk <= 0x7B) return static_cast<uint16_t>(0x3A + (vk - 0x70));
    if (vk >= 0x7C && vk <= 0x87) return static_cast<uint16_t>(0x68 + (vk - 0x7C));
    // Numpad 1-9, 0
    if (vk >= 0x61 && vk <= 0x69) return static_cast<uint16_t>(0x59 + (vk - 0x61));
    if (vk == 0x60) return 0x62;

    switch (vk) {
    case 0x0D: return 0x28; // Enter
    case 0x1B: return 0x29; // Escape
    case 0x08: return 0x2A; // Backspace
    case 0x09: return 0x2B; // Tab
    case 0x20: return 0x2C; // Space
    case 0xBD: return 0x2D; // - _
    case 0xBB: return 0x2E; // = +
    case 0xDB: return 0x2F; // [ {
    case 0xDD: return 0x30; // ] }
    case 0xDC: return jis ? 0x89 : 0x31; // \ |   (JIS: the Yen key, International3)
    case 0xBA: return 0x33; // ; :
    case 0xDE: return 0x34; // ' "
    case 0xC0: return 0x35; // ` ~
    case 0xBC: return 0x36; // , <
    case 0xBE: return 0x37; // . >
    case 0xBF: return 0x38; // / ?
    case 0x14: return 0x39; // Caps Lock
    case 0x2C: return 0x46; // Print Screen
    case 0x91: return 0x47; // Scroll Lock
    case 0x13: return 0x48; // Pause
    case 0x2D: return 0x49; // Insert
    case 0x24: return 0x4A; // Home
    case 0x21: return 0x4B; // Page Up
    case 0x2E: return 0x4C; // Delete
    case 0x23: return 0x4D; // End
    case 0x22: return 0x4E; // Page Down
    case 0x27: return 0x4F; // Right
    case 0x25: return 0x50; // Left
    case 0x28: return 0x51; // Down
    case 0x26: return 0x52; // Up
    case 0x90: return 0x53; // Num Lock
    case 0x6F: return 0x54; // Numpad /
    case 0x6A: return 0x55; // Numpad *
    case 0x6D: return 0x56; // Numpad -
    case 0x6B: return 0x57; // Numpad +
    case 0x6E: return 0x63; // Numpad .
    case 0xE2: return 0x87; // the key beside Right Shift on a JIS board (Ro); non-US \ on ISO (0x64)
    case 0x5D: return 0x65; // Menu / Application

    // JIS-only keys
    case 0x15: return 0x88; // Kana (Hiragana/Katakana), International2
    case 0x1C: return 0x8A; // Convert (Henkan), International4
    case 0x1D: return 0x8B; // NonConvert (Muhenkan), International5

    // Modifiers (the generic ones the browser may send stand for the left key)
    case 0x10: // Shift
    case 0xA0: return 0xE1;
    case 0xA1: return 0xE5;
    case 0x11: // Ctrl
    case 0xA2: return 0xE0;
    case 0xA3: return 0xE4;
    case 0x12: // Alt
    case 0xA4: return 0xE2;
    case 0xA5: return 0xE6;
    case 0x5B: return 0xE3; // Left Win
    case 0x5C: return 0xE7; // Right Win
    default: return 0;
    }
}

/// US-layout typing of one ASCII character: the Usage and whether Shift is
/// needed. Returns false for anything that is not printable ASCII.
inline bool asciiToHid(char c, uint16_t& usage, bool& shift)
{
    shift = false;
    if (c >= 'a' && c <= 'z') { usage = static_cast<uint16_t>(0x04 + (c - 'a')); return true; }
    if (c >= 'A' && c <= 'Z') { usage = static_cast<uint16_t>(0x04 + (c - 'A')); shift = true; return true; }
    if (c >= '1' && c <= '9') { usage = static_cast<uint16_t>(0x1E + (c - '1')); return true; }
    if (c == '0') { usage = 0x27; return true; }
    static constexpr char kShifted[] = ")!@#$%^&*(";
    for (int i = 0; i < 10; ++i) {
        if (c == kShifted[i]) { usage = i == 0 ? uint16_t{0x27} : static_cast<uint16_t>(0x1E + i - 1); shift = true; return true; }
    }
    struct Pair { char plain; char shifted; uint16_t usage; };
    static constexpr Pair kPairs[] = {
        {'-', '_', 0x2D}, {'=', '+', 0x2E}, {'[', '{', 0x2F}, {']', '}', 0x30},
        {'\\', '|', 0x31}, {';', ':', 0x33}, {'\'', '"', 0x34}, {'`', '~', 0x35},
        {',', '<', 0x36}, {'.', '>', 0x37}, {'/', '?', 0x38},
    };
    for (const Pair& p : kPairs) {
        if (c == p.plain) { usage = p.usage; return true; }
        if (c == p.shifted) { usage = p.usage; shift = true; return true; }
    }
    if (c == ' ') { usage = 0x2C; return true; }
    if (c == '\n') { usage = 0x28; return true; }
    if (c == '\t') { usage = 0x2B; return true; }
    return false;
}

} // namespace mw::native::input

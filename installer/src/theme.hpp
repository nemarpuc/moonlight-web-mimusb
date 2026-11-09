// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
//
// The window's look: dark cards on near-black, one blue accent. Fixed; there is
// nothing to configure. The palette is aoahid_player's dark theme.
#pragma once

#include <imgui.h>

#include <string>

namespace ui {

void setup(float dpi_scale);
// A length in the window's pixels.
[[nodiscard]] float px(float value);

namespace color {
inline constexpr ImU32 background = IM_COL32(0x0D, 0x11, 0x17, 255);
inline constexpr ImU32 card = IM_COL32(0x16, 0x1B, 0x22, 255);
inline constexpr ImU32 field = IM_COL32(0x21, 0x26, 0x2D, 255);
inline constexpr ImU32 field_hover = IM_COL32(0x30, 0x36, 0x3D, 255);
inline constexpr ImU32 border = IM_COL32(0x30, 0x36, 0x3D, 255);
inline constexpr ImU32 text = IM_COL32(0xF0, 0xF6, 0xFC, 255);
inline constexpr ImU32 dim = IM_COL32(0x9D, 0xA7, 0xB3, 255);
inline constexpr ImU32 accent = IM_COL32(0x38, 0x8B, 0xFD, 255);
inline constexpr ImU32 success = IM_COL32(0x3F, 0xB9, 0x50, 255);
inline constexpr ImU32 warning = IM_COL32(0xD2, 0x99, 0x22, 255);
inline constexpr ImU32 danger = IM_COL32(0xF8, 0x51, 0x49, 255);
} // namespace color

// A rounded panel. Everything up to card_end() is drawn inside it.
void card_begin(const char* id);
void card_end();

// A small caption in capitals above a group.
void caption(const char* text);
// Greyed text that wraps at the card's edge.
void dim(const char* text);
// A pill with a dot and a word, in `tone`.
void pill(const char* text, ImU32 tone);
// The main action: filled with the accent. Returns true when clicked.
bool primary_button(const char* label, float width = 0.0F);
// A quiet button.
bool button(const char* label, float width = 0.0F);
// A caption over a full-width text field.
bool field(const char* label, const char* id, std::string& value, float width = -1.0F);

} // namespace ui

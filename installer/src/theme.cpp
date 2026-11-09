// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
#include "theme.hpp"

#include <imgui_stdlib.h>

#include <cstdio>
#include <fstream>

// The UI font, compiled in (see CMakeLists.txt).
extern const unsigned char mwi_font_data[];
extern const unsigned int mwi_font_size;

namespace ui {
namespace {

float g_scale = 1.0F;
constexpr float font_body = 15.0F;

ImVec4 vec(const ImU32 value) { return ImGui::ColorConvertU32ToFloat4(value); }

ImU32 with_alpha(const ImU32 value, const unsigned alpha) {
    return (value & 0x00FFFFFFU) | (alpha << 24);
}

// A font for Japanese (and other CJK) text that the system already has, so a
// user name in a path shows as letters. Not embedded: it would be several MB.
std::string cjk_font_path() {
    const auto exists = [](const std::string& path) { return std::ifstream(path, std::ios::binary).good(); };
#if defined(_WIN32)
    for (const char* path : {"C:\\Windows\\Fonts\\Meiryo.ttc", "C:\\Windows\\Fonts\\YuGothM.ttc",
                             "C:\\Windows\\Fonts\\msgothic.ttc"})
        if (exists(path))
            return path;
#elif defined(__APPLE__)
    for (const char* path : {"/System/Library/Fonts/PingFang.ttc",
                             "/System/Library/Fonts/Hiragino Sans GB.ttc"})
        if (exists(path))
            return path;
#else
    std::string result;
    if (FILE* pipe = popen("fc-match -f '%{file}' ':lang=ja' 2>/dev/null", "r")) {
        char buffer[512];
        if (std::fgets(buffer, sizeof buffer, pipe) != nullptr)
            result = buffer;
        pclose(pipe);
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    if (!result.empty() && exists(result))
        return result;
#endif
    return {};
}

} // namespace

float px(const float value) { return value * g_scale; }

void setup(const float dpi_scale) {
    g_scale = dpi_scale;
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    config.OversampleH = 2;
    std::snprintf(config.Name, sizeof config.Name, "Roboto Medium");
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(mwi_font_data),
                                   static_cast<int>(mwi_font_size), font_body, &config);
    // Glyphs are rasterised on demand, so no range has to be named.
    const std::string cjk = cjk_font_path();
    if (!cjk.empty()) {
        ImFontConfig merge;
        merge.MergeMode = true;
        merge.OversampleH = 2;
        io.Fonts->AddFontFromFileTTF(cjk.c_str(), font_body, &merge);
    }

    ImGuiStyle style;
    style.WindowPadding = ImVec2(0, 0);
    style.FramePadding = ImVec2(12, 8);
    style.ItemSpacing = ImVec2(10, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.ScrollbarSize = 9;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 0;
    style.WindowRounding = 0;
    style.ChildRounding = 12;
    style.FrameRounding = 8;
    style.PopupRounding = 10;
    style.ScrollbarRounding = 5;
    style.GrabRounding = 6;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = vec(color::text);
    c[ImGuiCol_TextDisabled] = vec(color::dim);
    c[ImGuiCol_WindowBg] = vec(color::background);
    c[ImGuiCol_ChildBg] = vec(color::card);
    c[ImGuiCol_PopupBg] = vec(color::card);
    c[ImGuiCol_Border] = vec(color::border);
    c[ImGuiCol_FrameBg] = vec(color::field);
    c[ImGuiCol_FrameBgHovered] = vec(color::field_hover);
    c[ImGuiCol_FrameBgActive] = vec(color::field_hover);
    c[ImGuiCol_Button] = vec(color::field);
    c[ImGuiCol_ButtonHovered] = vec(color::field_hover);
    c[ImGuiCol_ButtonActive] = vec(color::border);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = vec(color::border);
    c[ImGuiCol_TextSelectedBg] = vec(with_alpha(color::accent, 110));
    c[ImGuiCol_NavCursor] = vec(color::accent);

    style.ScaleAllSizes(dpi_scale);
    style.FontSizeBase = font_body;
    style.FontScaleDpi = dpi_scale;
    ImGui::GetStyle() = style;
}

void card_begin(const char* const id) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(20), px(16)));
    ImGui::BeginChild(id, ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
}

void card_end() { ImGui::EndChild(); }

void caption(const char* const text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color::dim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void dim(const char* const text) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::PushStyleColor(ImGuiCol_Text, color::dim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
}

void pill(const char* const text, const ImU32 tone) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    const float pad_x = px(12);
    const float pad_y = px(5);
    const float dot = px(4);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 box(size.x + pad_x * 2 + dot * 2 + px(6), size.y + pad_y * 2);
    ImDrawList* const list = ImGui::GetWindowDrawList();
    const float round = box.y / 2;
    list->AddRectFilled(origin, ImVec2(origin.x + box.x, origin.y + box.y), with_alpha(tone, 40), round);
    list->AddRect(origin, ImVec2(origin.x + box.x, origin.y + box.y), with_alpha(tone, 150), round);
    list->AddCircleFilled(ImVec2(origin.x + pad_x + dot, origin.y + box.y / 2), dot, tone);
    list->AddText(ImVec2(origin.x + pad_x + dot * 2 + px(6), origin.y + pad_y), color::text, text);
    ImGui::Dummy(box);
}

bool primary_button(const char* const label, const float width) {
    ImGui::PushStyleColor(ImGuiCol_Button, color::accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0x5C, 0xA2, 0xFE, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0x2E, 0x72, 0xD2, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, color::background);
    const bool pressed = ImGui::Button(label, ImVec2(width, 0));
    ImGui::PopStyleColor(4);
    return pressed;
}

bool button(const char* const label, const float width) {
    return ImGui::Button(label, ImVec2(width, 0));
}

bool field(const char* const label, const char* const id, std::string& value, const float width) {
    // Same text offset in every cell, so the columns of a table line up.
    ImGui::AlignTextToFramePadding();
    caption(label);
    // Close the gap the padding above leaves under the caption.
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().FramePadding.y * 1.5F);
    ImGui::SetNextItemWidth(width);
    return ImGui::InputText(id, &value);
}

} // namespace ui

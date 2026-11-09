// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
//
// The installer: a window (or, with an argument, a command line) that puts the
// add-on build from bin/ in place of the official MoonlightWeb, and back.
#include "app.hpp"
#include "theme.hpp"

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

const char* state_word(const mwi::State state) {
    switch (state) {
    case mwi::State::Absent: return "Not found";
    case mwi::State::Official: return "Official build";
    case mwi::State::AddOn: return "Add-on installed";
    }
    return "";
}

ImU32 state_tone(const mwi::State state) {
    switch (state) {
    case mwi::State::Absent: return ui::color::warning;
    case mwi::State::Official: return ui::color::dim;
    case mwi::State::AddOn: return ui::color::success;
    }
    return ui::color::dim;
}

void draw_install_card(mwi::App& app, const mwi::State state, const bool busy) {
    ui::card_begin("install");
    ui::caption("MOONLIGHTWEB ON THIS PC");
    ImGui::Spacing();
    ui::pill(state_word(state), state_tone(state));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    if (!app.manifest.version.empty())
        ImGui::TextDisabled("release %s", app.manifest.version.c_str());

    ImGui::Spacing();
    ui::field("Install folder", "##dir", app.dir_text);
    if (!app.manifest_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ui::color::danger);
        ImGui::TextWrapped("%s", app.manifest_error.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::BeginDisabled(busy || !app.manifest_error.empty());
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    if (ui::primary_button(state == mwi::State::AddOn ? "Reinstall" : "Install", half))
        mwi::start_install(app);
    ImGui::SameLine();
    ImGui::BeginDisabled(state != mwi::State::AddOn);
    if (ui::button("Back to official", half))
        mwi::start_rollback(app);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ui::card_end();
}

void draw_device_card(mwi::App& app) {
    ui::card_begin("device");
    ui::caption("BOARD");
    ImGui::Spacing();
    if (!app.config_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ui::color::danger);
        ImGui::TextWrapped("%s", app.config_error.c_str());
        ImGui::PopStyleColor();
        if (ui::button("Read again"))
            mwi::reload_config(app);
        ui::card_end();
        return;
    }
    if (ImGui::BeginTable("pairs", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ui::field("Serial port", "##port", app.fields.port);
        ImGui::TableNextColumn();
        ui::field("Baud (0 with auto)", "##baud", app.fields.baud);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ui::field("USB VID", "##vid", app.fields.vid);
        ImGui::TableNextColumn();
        ui::field("USB PID", "##pid", app.fields.pid);
        ImGui::EndTable();
    }

    ui::field("Manufacturer", "##manufacturer", app.fields.manufacturer);
    ui::field("Product", "##product", app.fields.product);
    ui::field("Serial number (may be empty)", "##serial", app.fields.serial);

    ImGui::Spacing();
    if (ui::primary_button("Save mimhid.json"))
        mwi::save_config(app);
    ImGui::SameLine();
    if (ui::button("Discard changes"))
        mwi::reload_config(app);
    if (!app.config_note.empty())
        ui::dim(app.config_note.c_str());
    ui::dim(app.config_exists ? "Only these keys change; the rest of the file stays as it is."
                              : "No mimhid.json yet: saving makes one from the example.");
    ImGui::TextDisabled("%s", mwi::utf8(app.config_path).c_str());
    ui::card_end();
}

void draw_log(mwi::App& app, const bool busy) {
    std::vector<std::string> lines;
    std::string error;
    {
        const std::lock_guard lock(app.mutex);
        lines = app.log;
        error = app.last_error;
    }
    if (lines.empty() && !busy)
        return;
    ui::card_begin("log");
    ui::caption(busy ? "WORKING..." : "LOG");
    const std::size_t first = lines.size() > 6 ? lines.size() - 6 : 0;
    for (std::size_t i = first; i < lines.size(); ++i) {
        const bool bad = lines[i].rfind("error:", 0) == 0;
        ImGui::PushStyleColor(ImGuiCol_Text, bad ? ui::color::danger : ui::color::text);
        ImGui::TextWrapped("%s", lines[i].c_str());
        ImGui::PopStyleColor();
    }
    ui::card_end();
}

void draw_window(mwi::App& app, GLFWwindow* const window) {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width), static_cast<float>(height)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::px(24), ui::px(22)));
    ImGui::Begin("##main", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();

    mwi::State state;
    {
        const std::lock_guard lock(app.mutex);
        state = app.state;
    }
    const bool busy = app.busy.load();

    ImGui::PushStyleColor(ImGuiCol_Text, ui::color::accent);
    ImGui::SetWindowFontScale(1.0F);
    ImGui::TextUnformatted("moonlight-web-mimhid");
    ImGui::PopStyleColor();
    ui::dim("Keyboard and mouse of a Moonlight-Web session, through a libmimhid board.");
    ImGui::Spacing();

    draw_install_card(app, state, busy);
    draw_device_card(app);
    draw_log(app, busy);
    ImGui::End();
}

// installer status | install | rollback, for a machine with no screen.
int run_cli(mwi::App& app, const std::string& command) {
    mwi::refresh_state(app);
    if (command == "status") {
        std::printf("%s\n", state_word(app.state));
        return 0;
    }
    if (command != "install" && command != "rollback") {
        std::fprintf(stderr, "usage: installer [status | install | rollback]\n");
        return 2;
    }
    if (!app.manifest_error.empty()) {
        std::fprintf(stderr, "error: %s\n", app.manifest_error.c_str());
        return 1;
    }
    if (command == "install")
        mwi::start_install(app);
    else
        mwi::start_rollback(app);
    while (app.busy.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::lock_guard lock(app.mutex);
    for (const std::string& line : app.log)
        std::printf("%s\n", line.c_str());
    return app.last_error.empty() ? 0 : 1;
}

} // namespace

int main(const int argc, char** const argv) {
    static mwi::App app; // detached workers refer to it until the process ends
    mwi::init(app);
    if (argc > 1)
        return run_cli(app, argv[1]);

    if (glfwInit() == GLFW_FALSE)
        return 1;
#ifdef __APPLE__
    const char* shader_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char* shader_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(560, 880, "moonlight-web-mimhid installer", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    float scale = 1.0F;
    float scale_y = 1.0F;
    glfwGetWindowContentScale(window, &scale, &scale_y);
    ui::setup(std::clamp(scale, 1.0F, 4.0F));
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(shader_version);

    // Hashing every installed file takes a moment: not on the UI thread.
    std::thread([] { mwi::refresh_state(app); }).detach();

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        // Idle: wake only for input; while a job runs, redraw at 20 Hz.
        if (app.busy.load())
            glfwWaitEventsTimeout(0.05);
        else
            glfwWaitEventsTimeout(0.5);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_window(app, window);
        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.05F, 0.07F, 0.09F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

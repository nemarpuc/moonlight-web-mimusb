// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimhid contributors
#include "platform.hpp"

#include <cstdlib>
#include <fstream>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace mwi {
namespace {

std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

[[maybe_unused]] fs::path home() { return from_utf8(env("HOME")); }

#if !defined(_WIN32) && !defined(__APPLE__)
fs::path desktop_entry() {
    const std::string data = env("XDG_DATA_HOME");
    const fs::path base = data.empty() ? home() / ".local/share" : from_utf8(data);
    return base / "applications/moonlight-web-mimhid.desktop";
}
#endif

} // namespace

fs::path program_dir() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return fs::path(buffer).parent_path();
#elif defined(__APPLE__)
    std::string buffer(4096, '\0');
    auto size = static_cast<std::uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        return fs::current_path();
    return fs::weakly_canonical(fs::path(buffer.c_str())).parent_path();
#else
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() : exe.parent_path();
#endif
}

fs::path config_dir() {
    const std::string override_path = env("MW_MIMHID_CONFIG");
    if (!override_path.empty())
        return from_utf8(override_path).parent_path();
#if defined(_WIN32)
    return from_utf8(env("APPDATA")) / "MoonlightWeb" / "MoonlightWeb";
#elif defined(__APPLE__)
    return home() / "Library/Application Support/MoonlightWeb/MoonlightWeb";
#else
    return home() / ".local/share/MoonlightWeb/MoonlightWeb";
#endif
}

Plan default_plan(const fs::path& release_dir) {
    Plan plan;
    plan.bin = release_dir / "bin";
#if defined(_WIN32)
    plan.dir = from_utf8(env("ProgramFiles")) / "MoonlightWeb";
    plan.state = plan.dir / "mimhid-install.txt";
    plan.mode = Mode::Merge;
    plan.probe = "MoonlightWeb.exe";
#elif defined(__APPLE__)
    plan.dir = "/Applications";
    plan.state = home() / "Library/Application Support/moonlight-web-mimhid/install.txt";
    plan.mode = Mode::Units;
    plan.probe = "MoonlightWeb.app";
#else
    plan.dir = home() / ".local/bin";
    plan.state = home() / ".local/share/moonlight-web-mimhid/install.txt";
    plan.mode = Mode::Units;
    plan.probe = "MoonlightWeb.AppImage";
#endif
    return plan;
}

void stop_app() {
#if defined(_WIN32)
    std::system("taskkill /F /IM MoonlightWeb.exe >nul 2>&1");
#elif defined(__APPLE__)
    std::system("pkill -x MoonlightWeb >/dev/null 2>&1");
#else
    std::system("pkill -x MoonlightWeb >/dev/null 2>&1; pkill -f '^/[^ ]*MoonlightWeb[.]AppImage' >/dev/null 2>&1");
#endif
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void after_install(const Plan& plan) {
    std::error_code ec;
#if defined(_WIN32)
    (void)plan;
#elif defined(__APPLE__)
    const std::string app = utf8(plan.dir / "MoonlightWeb.app");
    std::system(("xattr -dr com.apple.quarantine '" + app + "' >/dev/null 2>&1").c_str());
#else
    const fs::path image = plan.dir / "MoonlightWeb.AppImage";
    fs::permissions(image, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                               fs::perms::others_read | fs::perms::others_exec,
                    ec);
    fs::create_directories(desktop_entry().parent_path(), ec);
    std::ofstream(desktop_entry())
        << "[Desktop Entry]\nType=Application\nName=MoonlightWeb (mimhid)\nExec=\"" << utf8(image)
        << "\"\nTerminal=false\nCategories=Network;\n";
#endif
}

void after_rollback(const Plan& plan) {
    (void)plan;
#if !defined(_WIN32) && !defined(__APPLE__)
    std::error_code ec;
    fs::remove(desktop_entry(), ec);
#endif
}

} // namespace mwi

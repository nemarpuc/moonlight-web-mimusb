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

#include "IInputSink.h"
#include "MimUsbConfig.h"

#include <bitset>
#include <memory>
#include <mutex>

namespace mw::native::input {

class MimUsbBoard;

/// Presents the viewer's keyboard and mouse to a target as a real USB device,
/// through libmimusb boards on serial ports — "physical" input instead of
/// SendInput / uinput / CGEvent. A board's USB side may be plugged into the very
/// PC that runs this server.
///
/// A decorator: it wraps the sink the platform already built. Whatever is not
/// routed to a board — a class no board carries, the pads, Ctrl+Alt+Del, every
/// setter — goes to that sink unchanged. A board that cannot be opened (or that
/// goes away) drops only its own classes back to it, so a missing board costs a
/// log line, not the session; a background thread tries again every 5 seconds
/// and the classes move back to the board once it answers.
///
/// Guests are never routed to a board: the administrator-window gate lives in
/// the OS sink, and a physical device would walk around it.
class MimUsbInput final : public IInputSink
{
public:
    MimUsbInput(std::unique_ptr<IInputSink> osSink, const MimUsbConfig& config);
    ~MimUsbInput() override;

    bool start(std::string& error) override;
    void stop() override;
    void inject(const InputEvent& event) override;

    void setDisplayRect(int left, int top, int right, int bottom) override;
    void setDesktopRect(int left, int top, int right, int bottom) override;
    void setAllowElevated(bool allow) override;
    void setGateCallback(InputGateCallback callback) override;
    bool releaseBlock() override;
    bool showsPointer() override;

private:
    bool routesKeyboard() const { return m_Keyboard && m_AllowElevated; }
    bool routesMouse() const { return m_Mouse && m_AllowElevated; }
    void injectKey(const InputEvent& event, bool down);
    void injectChar(const InputEvent& event, bool down);
    void injectText(const std::string& text);
    void injectMouseAbsolute(const InputEvent& event);
    void injectScroll(int amount, bool horizontal);
    void pressUsage(uint16_t usage, bool down, bool resync);
    void releaseEverything();
    void refreshRouting(); ///< picks up boards that came or went since the last event

    std::unique_ptr<IInputSink> m_Os;
    MimUsbConfig m_Config;
    std::shared_ptr<MimUsbBoard> m_Keyboard; ///< the board that carries the keyboard, if any
    std::shared_ptr<MimUsbBoard> m_Mouse;    ///< the board that carries the mouse, if any
    bool m_Jis = false;
    uint32_t m_MouseButtonCount = 5;
    bool m_AllowElevated = false;
    bool m_Registered = false; ///< counted as a user of the boards, until stop()
    uint64_t m_Generation = 0; ///< the Registry state this sink last looked at

    // Serialises injection: inject() may arrive from any thread.
    std::mutex m_Mutex;
    std::bitset<256> m_KeysDown;    ///< Usages this session holds
    std::bitset<256> m_CharShifted; ///< chars that were typed with an added Shift
    uint8_t m_ButtonsDown = 0;      ///< bit n-1 = HID button n
    int m_WheelRemainder = 0;
    int m_PanRemainder = 0;
    bool m_HaveAbsolute = false;
    int m_LastAbsX = 0;
    int m_LastAbsY = 0;
};

#ifdef MOONLIGHTWEB_MIMUSB
/// What a platform session calls where it builds its input sink: the sink
/// itself when mimusb.json asks for nothing, otherwise the sink inside a
/// MimUsbInput. @p allowElevated is SessionConfig::allowElevatedInput — only
/// the owner is routed to a board.
std::unique_ptr<IInputSink> wrapWithMimUsb(std::unique_ptr<IInputSink> osSink, bool allowElevated);
#else
inline std::unique_ptr<IInputSink> wrapWithMimUsb(std::unique_ptr<IInputSink> osSink, bool)
{
    return osSink;
}
#endif

} // namespace mw::native::input

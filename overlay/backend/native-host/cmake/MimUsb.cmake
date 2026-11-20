# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimusb contributors
# Included from the end of backend/native-host/CMakeLists.txt by tools/prepare.py,
# so upstream's file is touched by one appended line and nothing else.
# The target mw-native-host is upstream's.

# Physical HID output through libmimusb (moonlight-web-mimusb add-on): a
# microcontroller board on a serial port presents the viewer's keyboard and
# mouse as a real USB device. Opt-in: -DMW_MIMUSB=ON -DMW_MIMUSB_DIR=<libmimusb>.
option(MW_MIMUSB "Build the libmimusb physical-HID input backend" OFF)
set(MW_MIMUSB_DIR "" CACHE PATH "Path to a libmimusb checkout (MIT)")
if(MW_MIMUSB)
    if(NOT MW_MIMUSB_DIR OR NOT EXISTS "${MW_MIMUSB_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "MW_MIMUSB=ON needs MW_MIMUSB_DIR pointing at a libmimusb checkout")
    endif()
    set(MIMUSB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MIMUSB_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(MIMUSB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(MIMUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MW_MIMUSB_DIR}" "${CMAKE_BINARY_DIR}/libmimusb" EXCLUDE_FROM_ALL)
    target_sources(mw-native-host PRIVATE
        src/input/MimUsbConfig.cpp
        src/input/MimUsbInput.cpp)
    target_link_libraries(mw-native-host PRIVATE mimusb_static)
    target_compile_definitions(mw-native-host PRIVATE MOONLIGHTWEB_MIMUSB)
endif()

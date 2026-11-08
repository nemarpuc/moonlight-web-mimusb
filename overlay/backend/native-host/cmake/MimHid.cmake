# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimhid contributors
# Included from the end of backend/native-host/CMakeLists.txt by tools/prepare.py,
# so upstream's file is touched by one appended line and nothing else.
# The target mw-native-host is upstream's.

# Physical HID output through libmimhid (moonlight-web-mimhid add-on): a
# microcontroller board on a serial port presents the viewer's keyboard and
# mouse as a real USB device. Opt-in: -DMW_MIMHID=ON -DMW_MIMHID_DIR=<libmimhid>.
option(MW_MIMHID "Build the libmimhid physical-HID input backend" OFF)
set(MW_MIMHID_DIR "" CACHE PATH "Path to a libmimhid checkout (MIT)")
if(MW_MIMHID)
    if(NOT MW_MIMHID_DIR OR NOT EXISTS "${MW_MIMHID_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "MW_MIMHID=ON needs MW_MIMHID_DIR pointing at a libmimhid checkout")
    endif()
    set(MIMHID_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MIMHID_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(MIMHID_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(MIMHID_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${MW_MIMHID_DIR}" "${CMAKE_BINARY_DIR}/libmimhid" EXCLUDE_FROM_ALL)
    target_sources(mw-native-host PRIVATE
        src/input/MimHidConfig.cpp
        src/input/MimHidInput.cpp)
    target_link_libraries(mw-native-host PRIVATE mimhid_static)
    target_compile_definitions(mw-native-host PRIVATE MOONLIGHTWEB_MIMHID)
endif()

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 the moonlight-web-mimhid contributors
#
# The installer: one program per system that puts the add-on build from the
# release's bin/ folder in place of the official MoonlightWeb. Dear ImGui and
# GLFW are fetched at pinned, hashed releases and linked statically.
cmake_minimum_required(VERSION 3.21)
project(mwi_installer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MWI_BUILD_GUI "Build the window" ON)
option(MWI_BUILD_TESTS "Build the tests" OFF)
option(MWI_WERROR "Warnings are errors" OFF)

function(mwi_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8)
    if(MWI_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wconversion -Wsign-conversion)
    if(MWI_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

add_library(mwi_core STATIC src/sha256.cpp src/core.cpp src/config.cpp src/platform.cpp)
target_include_directories(mwi_core PUBLIC src)
mwi_warnings(mwi_core)

if(MWI_BUILD_TESTS)
  enable_testing()
  add_executable(mwi_test tests/test_core.cpp)
  target_link_libraries(mwi_test PRIVATE mwi_core)
  mwi_warnings(mwi_test)
  add_test(NAME mwi.unit COMMAND mwi_test)
endif()

if(NOT MWI_BUILD_GUI)
  return()
endif()

include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(imgui
  URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9.tar.gz
  URL_HASH SHA256=af97ed649182c39314320514a672b82008ab462b9293fe23d37b30bfa5d05519
)
FetchContent_Declare(glfw
  URL https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.zip
  URL_HASH SHA256=ea79bc5feffc254c87291980c2d0bce9acebb68c4983b79f961dcd2cb8a611a0
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(glfw imgui)
find_package(OpenGL REQUIRED)

add_library(mwi_imgui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
  "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(mwi_imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends"
                                            "${imgui_SOURCE_DIR}/misc/cpp")
target_link_libraries(mwi_imgui PUBLIC glfw OpenGL::GL ${CMAKE_DL_LIBS})

# The UI font (Roboto Medium, Apache-2.0, shipped with Dear ImGui) is compiled in.
set(font_source "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf")
set(font_cpp "${CMAKE_CURRENT_BINARY_DIR}/embedded_font.cpp")
add_custom_command(
  OUTPUT "${font_cpp}"
  COMMAND "${CMAKE_COMMAND}" "-DINPUT=${font_source}" "-DOUTPUT=${font_cpp}"
          -DSYMBOL=mwi_font -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
  DEPENDS "${font_source}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
  COMMENT "Embedding Roboto-Medium.ttf"
  VERBATIM)

add_executable(mwi_installer src/main.cpp src/app.cpp src/theme.cpp "${font_cpp}")
set_target_properties(mwi_installer PROPERTIES OUTPUT_NAME installer WIN32_EXECUTABLE ON)
target_link_libraries(mwi_installer PRIVATE mwi_core mwi_imgui)
mwi_warnings(mwi_installer)
if(APPLE)
  target_compile_definitions(mwi_installer PRIVATE GL_SILENCE_DEPRECATION)
endif()
if(MSVC)
  target_link_options(mwi_installer PRIVATE /ENTRY:mainCRTStartup
    # The official build lives in Program Files: ask for the rights up front.
    "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Built on an old distribution for its glibc: carry the C++ runtime along.
  target_link_options(mwi_installer PRIVATE -static-libstdc++ -static-libgcc)
endif()

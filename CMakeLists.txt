# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

cmake_minimum_required(VERSION 3.28)
project(cps2recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# The release's version (VERSION): online play's two copies must have the same (port/netplay.cpp)
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" RECOMP_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")

# Game selection: the game id names a directory under games/ and the generated code under work/
set(GAME "vampire_savior" CACHE STRING "Game id (directory under games/)")
set(GAME_DIR "${CMAKE_SOURCE_DIR}/../games/${GAME}" CACHE PATH "The game's directory (game.cfg, src/)")
set(WORK_DIR "${CMAKE_SOURCE_DIR}/../work/${GAME}" CACHE PATH "Generated code directory")

# (macOS: 3.4.16 or later. 3.4.14's Metal GPU device takes a frame's fence still being worked on for one signalled, so
# after the first frames every picture is skipped and the window stays black while the game runs and sounds)
if (APPLE)
    find_package(SDL3 3.4.16 REQUIRED CONFIG)
else ()
    find_package(SDL3 REQUIRED CONFIG)
endif ()
find_package(ZLIB REQUIRED)

# A Neo Geo game (game.cfg's board = neogeo; this fork): its own board (machine/neogeo*.cpp, video/neogeo_video.cpp) and
# its port, named as the game: the shared pieces of the collection's ports (the menu, the settings, the presenter, the
# HUD, the game's sources' registry) around it, in a window or with none, in the 68000's interpreter
# (port/neogeo_port.cpp, neogeo_window.cpp, neogeo_main.cpp; openspec changes neogeo-boot, neogeo-window-sound,
# neogeo-frontend). None of the Capcom targets below are made for it.
file(STRINGS "${GAME_DIR}/game.cfg" GAME_BOARD REGEX "^board[ \t]*=")
if (GAME_BOARD MATCHES "neogeo")
    set(YMFM_OPN third_party/ymfm_opn.cpp third_party/ymfm_ssg.cpp third_party/ymfm_adpcm.cpp)
    set_source_files_properties(${YMFM_OPN} PROPERTIES COMPILE_OPTIONS "-w")
    file(GLOB NEOGEO_GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
    if (NOT NEOGEO_GAME_SOURCES)
        set(NEOGEO_GAME_SOURCES runtime/no_game.cpp)
    endif ()
    add_executable(${GAME} port/neogeo_main.cpp port/neogeo_port.cpp port/neogeo_window.cpp port/present.cpp port/menu.cpp
        port/hud.cpp recomp-common/menu.cpp recomp-common/menubar.cpp recomp-common/settings.cpp runtime/game.cpp runtime/neogeo_game.cpp
        machine/neogeo.cpp machine/neogeo_sound.cpp video/neogeo_video.cpp machine/m68k.cpp sound/z80.cpp machine/romset.cpp
        machine/kabuki.cpp machine/cps2crypt.cpp runtime/no_blocks.cpp runtime/no_z80_blocks.cpp ${YMFM_OPN}
        ${NEOGEO_GAME_SOURCES})
    target_include_directories(${GAME} PRIVATE runtime port machine "${WORK_DIR}/gen" "${GAME_DIR}")
    target_link_libraries(${GAME} PRIVATE SDL3::SDL3 ZLIB::ZLIB)
    target_compile_options(${GAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    if (WIN32)
        set_target_properties(${GAME} PROPERTIES WIN32_EXECUTABLE ON)
        get_filename_component(GAME_ICON "${GAME_DIR}/icon.ico" ABSOLUTE)
        if (EXISTS "${GAME_ICON}")
            enable_language(RC)
            file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" "1 ICON \"${GAME_ICON}\"\n")
            target_sources(${GAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
        endif ()
    endif ()
    return()
endif ()

# Translated game code: work/<game>/gen/native_*.cpp, written by tools/recomp.py.
# Without it the game still builds and runs everything in the interpreter.
file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/gen/native_*.cpp")
if (GEN_SOURCES)
    message(STATUS "cps2recomp: ${GAME}: translated code found in ${WORK_DIR}/gen")
else ()
    message(STATUS "cps2recomp: ${GAME}: no translated code yet (interpreter only)")
    set(GEN_SOURCES runtime/no_blocks.cpp)
endif ()

# The sound driver translated: work/<game>/gen/z80_*.cpp, written by tools/recompz80.py
file(GLOB GEN_Z80_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/gen/z80_*.cpp")
if (NOT GEN_Z80_SOURCES)
    set(GEN_Z80_SOURCES runtime/no_z80_blocks.cpp)
endif ()

set(MACHINE_SOURCES
    video/video.cpp
    sound/z80.cpp sound/qsound.cpp sound/driver.cpp sound/okim6295.cpp third_party/ymfm_opm.cpp
    machine/m68k.cpp machine/machine.cpp machine/cps1_sound.cpp machine/sound_board.cpp machine/soundtrack.cpp machine/eeprom.cpp machine/romset.cpp machine/cps2crypt.cpp machine/kabuki.cpp
    runtime/recomp.cpp runtime/recomp_z80.cpp
)
# The game's own sources (features, routines replaced by hand-written C++); without any, nothing is added
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
# ... and those of the games it shares sources with (game.cfg's "sources": another game made from the same program,
# whose sources name its places as At(0x...); the game's own family.h, in its folder, says where they are in it)
file(STRINGS "${GAME_DIR}/game.cfg" GAME_SHARES REGEX "^sources[ \t]*=")
if (GAME_SHARES)
    string(REGEX REPLACE "^sources[ \t]*=[ \t]*" "" GAME_SHARES "${GAME_SHARES}")
    separate_arguments(GAME_SHARES)
    foreach (other ${GAME_SHARES})
        file(GLOB shared CONFIGURE_DEPENDS "${GAME_DIR}/../${other}/src/*.cpp")
        list(APPEND GAME_SOURCES ${shared})
        list(APPEND GAME_INCLUDES "${GAME_DIR}/../${other}/src") # (for the game's own sources, which use them)
        message(STATUS "cps2recomp: ${GAME}: the sources of ${other} too")
    endforeach ()
endif ()
if (NOT GAME_SOURCES)
    set(GAME_SOURCES runtime/no_game.cpp)
endif ()
# (recomp-common/: the code the recomp ports share, a copy of recomp-common's; tools/sync-common.sh)
# GekkoNet (BSD-2-Clause): online play's rollback (port/online.h; this fork's), its library only, static, without asio
# (the port has its own UDP adapter): the same pinned release as neogeo-recomp's
include(FetchContent)
set(NO_ASIO_BUILD ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(gekkonet
    URL https://github.com/HeatXD/GekkoNet/archive/refs/tags/v20261024222910-9da2363.tar.gz
    URL_HASH SHA256=a9001dec8698120fe45b314527d181c403ee92e3a30a03d765d3e792b8c74e1c
    SOURCE_SUBDIR GekkoLib
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(gekkonet)
# (the version both players of an online match must have: VERSION's)
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" CPS2RECOMP_VERSION LIMIT_COUNT 1)

set(PORT_SOURCES port/main.cpp port/online.cpp port/frontend_sdl.cpp port/present.cpp port/shading.cpp port/zoom.cpp port/menu.cpp port/hud.cpp port/touch.cpp port/soundtrack.cpp port/netplay.cpp recomp-common/launcher/src/online.cpp recomp-common/launcher/src/run_program.cpp port/decoders.cpp recomp-common/menu.cpp recomp-common/menubar.cpp recomp-common/settings.cpp runtime/game.cpp runtime/board.cpp runtime/spare.cpp runtime/guest.cpp fight/frames.cpp fight/training.cpp fight/colours.cpp fight/colour_editor.cpp fight/battle.cpp fight/alternative.cpp fight/session.cpp fight/tag.cpp fight/marvel.cpp fight/guest_stages.cpp beat/modes.cpp beat/model.cpp model/model.cpp ${GAME_SOURCES})

# (the sound files' decoders, third_party/, are other projects' code, compiled as they are)
set_source_files_properties(port/decoders.cpp PROPERTIES COMPILE_OPTIONS "-w")
# (ymfm's published YM2151 implementation for the classic CPS-1 sound board)
set_source_files_properties(third_party/ymfm_opm.cpp PROPERTIES COMPILE_OPTIONS "-w")

# (macOS: handing the front to the launcher, port/front_macos.mm)
if (APPLE)
    enable_language(OBJCXX)
    list(APPEND PORT_SOURCES port/front_macos.mm)
endif ()

# The game on cps2recomp's machine (machine/); classic CPS-1 uses the credited ymfm YM2151 library.
# (Android: a library, which the game's activity loads and calls the main of: android/, tools/release/android.sh)
if (ANDROID)
    list(APPEND PORT_SOURCES port/android.cpp)
    add_library(${GAME} SHARED ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
else ()
    add_executable(${GAME} ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
endif ()
# The check build ('make check-blocks'): the same, with every translated block run beside the interpreter
# and compared (runtime/recomp.cpp)
add_executable(${GAME}-check EXCLUDE_FROM_ALL ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
target_compile_definitions(${GAME}-check PRIVATE CPS2RECOMP_CHECK)
# (on Windows the game is a windowed program: no console window opens beside it, whether the launcher starts it
# or a double click does)
if (WIN32)
    set_target_properties(${GAME} PROPERTIES WIN32_EXECUTABLE ON)
    # (and the game's icon, games/<id>/icon.ico, when it has one: the program's and its shortcuts')
    get_filename_component(GAME_ICON "${GAME_DIR}/icon.ico" ABSOLUTE)
    if (EXISTS "${GAME_ICON}")
        enable_language(RC)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" "1 ICON \"${GAME_ICON}\"\n")
        target_sources(${GAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    endif ()
endif ()
foreach (target ${GAME} ${GAME}-check)
    target_include_directories(${target} PRIVATE runtime port machine fight beat model "${WORK_DIR}/gen" "${GAME_DIR}" ${GAME_INCLUDES})
    target_link_libraries(${target} PRIVATE SDL3::SDL3 ZLIB::ZLIB GekkoNet)
    if (APPLE)
        target_link_libraries(${target} PRIVATE "-framework AppKit")
    endif ()
    if (ANDROID)
        target_link_libraries(${target} PRIVATE log)
    endif ()
    if (WIN32)
        target_link_libraries(${target} PRIVATE ws2_32 iphlpapi) # (online play's socket, and this machine's addresses)
    endif ()
    target_compile_definitions(${target} PRIVATE CPS2RECOMP_VERSION="${CPS2RECOMP_VERSION}")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-label -Wno-unused-variable -Wno-parentheses-equality)
    # (a char is signed, as on x86 and Apple's ARM, also on Linux ARM64, whose own is unsigned: every build does the
    # same sums)
    target_compile_options(${target} PRIVATE -fsigned-char)
    target_compile_definitions(${target} PRIVATE RECOMP_VERSION="${RECOMP_VERSION}")
endforeach ()

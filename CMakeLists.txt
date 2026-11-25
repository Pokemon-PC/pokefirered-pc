# pokefirered - native PC build (Windows/Linux/macOS), CMake + Clang + SDL2.
# The GBA ROM is still built with `make` (Makefile), unchanged and matching.
#
#   cmake -S src/platform_pc/tools -B build-tools -G Ninja && cmake --build build-tools
#   python src/platform_pc/scripts/gen_assets.py
#   cmake -B build-pc -G Ninja -DCMAKE_C_COMPILER=clang && cmake --build build-pc
#   cmake -B build-lg -G Ninja -DCMAKE_C_COMPILER=clang -DPFR_GAME_VERSION=LEAFGREEN
cmake_minimum_required(VERSION 3.20)
project(pokefirered_pc C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The PC port targets 64-bit hosts only.")
endif()

# FireRed and LeafGreen come from the same sources (-DFIRERED / -DLEAFGREEN,
# as GAME_VERSION in config.mk): one build directory and one executable each.
set(PFR_GAME_VERSION FIRERED CACHE STRING "FIRERED or LEAFGREEN")
set_property(CACHE PFR_GAME_VERSION PROPERTY STRINGS FIRERED LEAFGREEN)
if(PFR_GAME_VERSION STREQUAL "FIRERED")
    set(PFR_EXE_NAME pokefirered)
elseif(PFR_GAME_VERSION STREQUAL "LEAFGREEN")
    set(PFR_EXE_NAME pokeleafgreen)
else()
    message(FATAL_ERROR "PFR_GAME_VERSION must be FIRERED or LEAFGREEN")
endif()
# Revision 0, English: the decomp's default (config.mk).
set(PFR_VERSION_DEFS ${PFR_GAME_VERSION}=1 REVISION=0 ENGLISH=1)
message(STATUS "Game: ${PFR_GAME_VERSION} -> ${PFR_EXE_NAME}")

include(cmake/PfrSDL2.cmake)

set(PFR_PC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/platform_pc)

# --- low-address executables (same rationale as the HeartGold port) -------
# The game keeps code/data addresses in 32-bit words (.4byte pointers in the
# script bytecode, u32 casts, task args): link at 0x10000000, without ASLR.
function(pfr_low_address_link target)
    if(WIN32)
        target_link_options(${target} PRIVATE
            -Wl,--image-base=0x10000000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va)
    elseif(APPLE)
        target_link_options(${target} PRIVATE -Wl,-pagezero_size,0x1000 -Wl,-image_base,0x10000000
            -Wl,-no_pie -Wl,-no_fixup_chains)
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
    else()
        include(CheckLinkerFlag)
        check_linker_flag(C "-Wl,-Ttext-segment=0x10000000" PFR_LD_TTEXT_SEGMENT)
        target_link_options(${target} PRIVATE -no-pie)
        if(PFR_LD_TTEXT_SEGMENT)
            target_link_options(${target} PRIVATE -Wl,-Ttext-segment=0x10000000)
        else()
            target_link_options(${target} PRIVATE -Wl,--image-base=0x10000000)
        endif()
        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
    endif()
endfunction()

# --- HAL: GBA hardware reproduced on the host -----------------------------
add_library(pfr_hal STATIC
    ${PFR_PC_DIR}/log_pc.c
    ${PFR_PC_DIR}/hw_pc.c
    ${PFR_PC_DIR}/bios_pc.c
    ${PFR_PC_DIR}/gpu_pc.c
    ${PFR_PC_DIR}/dma_pc.c
    ${PFR_PC_DIR}/irq_pc.c
    ${PFR_PC_DIR}/video_pc.c
    ${PFR_PC_DIR}/crash_pc.c
    ${PFR_PC_DIR}/timer_pc.c
    ${PFR_PC_DIR}/audio_pc.c
    ${PFR_PC_DIR}/nullread_pc.c
    ${PFR_PC_DIR}/config_pc.c
    ${PFR_PC_DIR}/rom_pc.c)
target_include_directories(pfr_hal PUBLIC ${PFR_PC_DIR})
target_compile_definitions(pfr_hal PUBLIC PLATFORM_PC=1 ${PFR_VERSION_DEFS})
target_link_libraries(pfr_hal PUBLIC pfr::SDL2)
if(WIN32)
    target_link_libraries(pfr_hal PUBLIC psapi dbghelp)
else()
    target_link_libraries(pfr_hal PUBLIC m)
endif()

# Flags for code that includes the game's headers.
add_library(pfr_game_flags INTERFACE)
# -iquote, not -I: include/ has malloc.h, strings.h... that would shadow the
# system headers.
target_compile_options(pfr_game_flags INTERFACE "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(pfr_game_flags INTERFACE MODERN=1 PLATFORM_PC=1 ${PFR_VERSION_DEFS})
target_compile_options(pfr_game_flags INTERFACE
    -fno-strict-aliasing -fwrapv -funsigned-char
    -fms-extensions                           # PC_PTR32 = __ptr32 __uptr
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
    -Wno-gnu-alignof-expression   # MODERN alignment asserts in gba/macro.h
    -Wno-macro-redefined)         # include/config.h sets NDEBUG itself

# HAL pieces that implement game APIs (same symbols as the files they
# replace in the PC build: agb_flash*.c), built with game headers.
# FireRed/LeafGreen cartridges have no real-time clock (no siirtc.c).
add_library(pfr_hal_game STATIC
    ${PFR_PC_DIR}/flash_pc.c)
target_link_libraries(pfr_hal_game PUBLIC pfr_hal PRIVATE pfr_game_flags)
# pfr_hal calls back into it (pc_flash_flush on crash, at VBlank): a cycle of
# static libraries, which CMake repeats on the link line for GNU ld.
target_link_libraries(pfr_hal INTERFACE pfr_hal_game)

# --- the game ---------------------------------------------------------
include(cmake/PfrGame.cmake)

# --- M2: hardware layer autotest ----------------------------------------
enable_testing()
add_executable(pfr_autotest ${PFR_PC_DIR}/tests/autotest_pc.c)
target_link_libraries(pfr_autotest PRIVATE pfr_hal_game pfr_hal pfr_game_flags)
pfr_low_address_link(pfr_autotest)
add_test(NAME hal_autotest
         COMMAND pfr_autotest ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

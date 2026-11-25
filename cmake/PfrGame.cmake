# The game's own sources (src/*.c), built for the host.
#
# Each file goes through the GBA Makefile's pipeline (cpp | preproc), run by
# src/platform_pc/scripts/pp_c.py, then is compiled as plain C. Files whose
# job is done by the PC HAL instead are left out (PFR_GAME_EXCLUDE).

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(PFR_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PFR_EXE_SUFFIX "")
if(WIN32)
    set(PFR_EXE_SUFFIX ".exe")
endif()
set(PFR_PREPROC ${PFR_ROOT}/tools/preproc/preproc${PFR_EXE_SUFFIX})
if(NOT EXISTS ${PFR_PREPROC})
    message(FATAL_ERROR "tools not built: cmake -S src/platform_pc/tools -B build-tools && cmake --build build-tools")
endif()
if(NOT EXISTS ${PFR_ROOT}/build/assets)
    message(WARNING "build/assets missing: run python src/platform_pc/scripts/gen_assets.py")
endif()

# Replaced by the HAL (same symbols, see src/platform_pc/*_pc.c).
set(PFR_GAME_EXCLUDE
    src/agb_flash.c src/agb_flash_1m.c src/agb_flash_mx.c   # flash_pc.c
    src/agb_flash_le.c
    src/isagbprn.c                                          # isagbprn_pc.c (AGBPrint -> log)
    # ARM-only link cable / wireless adapter code: no link on PC ("cable
    # unplugged"), the symbols they export are stubbed in link_pc.c.
    src/librfu_intr.c src/multiboot.c
)

file(GLOB PFR_GAME_SOURCES RELATIVE ${PFR_ROOT} ${PFR_ROOT}/src/*.c)
list(REMOVE_ITEM PFR_GAME_SOURCES ${PFR_GAME_EXCLUDE})

# Preprocessor flags, as CPPFLAGS in the Makefile (MODERN=1).
set(PFR_CPP_FLAGS
    -iquote ${PFR_ROOT}/include
    -iquote ${PFR_ROOT}/src/platform_pc
    -DMODERN=1 -DPLATFORM_PC=1
    -D${PFR_GAME_VERSION}=1 -DREVISION=0 -DENGLISH=1   # as CPPFLAGS (config.mk)
    -fms-extensions -funsigned-char    # same as the compile step (system headers test them)
    -Wno-trigraphs)

# All writable game variables go to one section (snapshot for soft reset,
# see src/platform_pc/main_pc.c). COFF: grouped section between markers.
if(WIN32)
    set(PFR_GAME_DATA_SECTION ".data$pfr1")
elseif(APPLE)
    set(PFR_GAME_DATA_SECTION "__DATA,__pfrgame")
else()
    set(PFR_GAME_DATA_SECTION "pfr_game")
endif()

set(PFR_PP_DIR ${CMAKE_CURRENT_BINARY_DIR}/pp)
set(PFR_GAME_PP_SOURCES)
foreach(src ${PFR_GAME_SOURCES})
    string(REPLACE "/" "__" flat ${src})
    set(out ${PFR_PP_DIR}/${flat})
    add_custom_command(
        OUTPUT ${out}
        COMMAND ${Python3_EXECUTABLE} ${PFR_ROOT}/src/platform_pc/scripts/pp_c.py
                --clang ${CMAKE_C_COMPILER} --preproc ${PFR_PREPROC} --root ${PFR_ROOT}
                --out ${out} --dep ${out}.d --data-section ${PFR_GAME_DATA_SECTION}
                -- ${PFR_CPP_FLAGS} ${PFR_ROOT}/${src}
        DEPENDS ${PFR_ROOT}/${src} ${PFR_ROOT}/src/platform_pc/scripts/pp_c.py
        DEPFILE ${out}.d
        COMMENT "preproc ${src}"
        VERBATIM)
    list(APPEND PFR_GAME_PP_SOURCES ${out})
endforeach()

add_library(pfr_game STATIC ${PFR_GAME_PP_SOURCES})
target_compile_options(pfr_game PRIVATE
    -fno-strict-aliasing -fwrapv -funsigned-char
    -fms-extensions                           # PC_PTR32 = __ptr32 __uptr
    -fno-zero-initialized-in-bss              # zeros too go to the game data section
    -w                                        # decompiled code: thousands of noise warnings
    -Werror=implicit-function-declaration
    -Werror=int-conversion
    -Werror=incompatible-pointer-types
    -Werror=return-type)
target_link_libraries(pfr_game PUBLIC pfr_hal_game pfr_hal)

# --- data written in assembly (scripts, maps, sound) -----------------------
# Same pipeline as the Makefile's DATA_ASM rule, assembled for the host by
# GNU as (pp_s.py explains why not Clang's assembler).
# llvm-mingw names its LLVM assembler "x86_64-w64-mingw32-as" too: look for
# every "as" on the PATH and keep the first one that is GNU as.
if(NOT PFR_GNU_AS)
    execute_process(COMMAND where as OUTPUT_VARIABLE _as_list ERROR_QUIET)
    if(NOT WIN32)
        execute_process(COMMAND sh -c "which -a as" OUTPUT_VARIABLE _as_list ERROR_QUIET)
    endif()
    string(REPLACE "
" ";" _as_list "${_as_list}")
    foreach(_as ${_as_list})
        string(STRIP "${_as}" _as)
        if(_as)
            execute_process(COMMAND "${_as}" --version OUTPUT_VARIABLE _v ERROR_QUIET)
            if(_v MATCHES "GNU assembler")
                set(PFR_GNU_AS "${_as}" CACHE FILEPATH "GNU as for the host")
                break()
            endif()
        endif()
    endforeach()
endif()
if(NOT PFR_GNU_AS)
    message(FATAL_ERROR "GNU as (binutils) not found: install binutils / WinLibs, or set PFR_GNU_AS")
endif()
message(STATUS "GNU as: ${PFR_GNU_AS}")
# ASFLAGS of the Makefile: --defsym FIRERED=1 --defsym REVISION=0 ...
set(PFR_AS_DEFSYMS --defsym ${PFR_GAME_VERSION}=1 --defsym REVISION=0 --defsym ENGLISH=1 --defsym MODERN=1)
file(GLOB PFR_DATA_ASM RELATIVE ${PFR_ROOT} ${PFR_ROOT}/data/*.s)
file(GLOB PFR_SONG_ASM RELATIVE ${PFR_ROOT} ${PFR_ROOT}/sound/songs/midi/*.s)
set(PFR_PP_S_FLAGS)
if(WIN32)
    set(PFR_PP_S_FLAGS --coff)
endif()
set(PFR_GAME_DATA_OBJS)
foreach(src ${PFR_DATA_ASM})
    string(REPLACE "/" "__" flat ${src})
    set(pps ${PFR_PP_DIR}/${flat})
    set(obj ${PFR_PP_DIR}/${flat}.obj)
    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${Python3_EXECUTABLE} ${PFR_ROOT}/src/platform_pc/scripts/pp_s.py ${PFR_PP_S_FLAGS}
                --clang ${CMAKE_C_COMPILER} --preproc ${PFR_PREPROC} --root ${PFR_ROOT}
                --out ${pps} --dep ${pps}.d
                -- -iquote ${PFR_ROOT}/include -DMODERN=1 -DPLATFORM_PC=1 ${PFR_ROOT}/${src}
        COMMAND ${PFR_GNU_AS} ${PFR_AS_DEFSYMS} -I ${PFR_ROOT} -I ${PFR_ROOT}/include -o ${obj} ${pps}
        DEPENDS ${PFR_ROOT}/${src} ${PFR_ROOT}/src/platform_pc/scripts/pp_s.py
        DEPFILE ${pps}.d
        WORKING_DIRECTORY ${PFR_ROOT}
        COMMENT "asm ${src}"
        VERBATIM)
    list(APPEND PFR_GAME_DATA_OBJS ${obj})
endforeach()
# Songs (mid2agb output): assembled directly with -I sound, as in
# audio_rules.mk, after the ARM -> x86 syntax fixes (pp_s.py --raw). Their
# common include, sound/MPlayDef.s, gets the same treatment.
set(PFR_SOUND_INC ${PFR_PP_DIR}/sound_inc)
add_custom_command(
    OUTPUT ${PFR_SOUND_INC}/MPlayDef.s
    COMMAND ${Python3_EXECUTABLE} ${PFR_ROOT}/src/platform_pc/scripts/pp_s.py ${PFR_PP_S_FLAGS} --raw
            --root ${PFR_ROOT} --out ${PFR_SOUND_INC}/MPlayDef.s --dep ${PFR_SOUND_INC}/MPlayDef.s.d
            -- ${PFR_ROOT}/sound/MPlayDef.s
    DEPENDS ${PFR_ROOT}/sound/MPlayDef.s ${PFR_ROOT}/src/platform_pc/scripts/pp_s.py
    VERBATIM)
foreach(src ${PFR_SONG_ASM})
    string(REPLACE "/" "__" flat ${src})
    set(pps ${PFR_PP_DIR}/${flat})
    set(obj ${pps}.obj)
    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${Python3_EXECUTABLE} ${PFR_ROOT}/src/platform_pc/scripts/pp_s.py ${PFR_PP_S_FLAGS} --raw
                --root ${PFR_ROOT} --out ${pps} --dep ${pps}.d -- ${PFR_ROOT}/${src}
        COMMAND ${PFR_GNU_AS} ${PFR_AS_DEFSYMS} -I ${PFR_SOUND_INC} -I ${PFR_ROOT}/sound -o ${obj} ${pps}
        DEPENDS ${PFR_ROOT}/${src} ${PFR_SOUND_INC}/MPlayDef.s ${PFR_ROOT}/src/platform_pc/scripts/pp_s.py
        WORKING_DIRECTORY ${PFR_ROOT}
        COMMENT "asm ${src}"
        VERBATIM)
    list(APPEND PFR_GAME_DATA_OBJS ${obj})
endforeach()
set_source_files_properties(${PFR_GAME_DATA_OBJS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

# --- the executable -----------------------------------------------------------
add_executable(${PFR_EXE_NAME}
    ${PFR_ROOT}/src/platform_pc/main_pc.c
    ${PFR_ROOT}/src/platform_pc/link_pc.c
    ${PFR_ROOT}/src/platform_pc/sound_pc.c
    ${PFR_ROOT}/src/platform_pc/layout_check_pc.c
    ${PFR_ROOT}/src/platform_pc/debug_pc.c
    ${PFR_ROOT}/src/platform_pc/isagbprn_pc.c
    ${PFR_GAME_DATA_OBJS})
target_link_libraries(${PFR_EXE_NAME} PRIVATE pfr_game pfr_hal_game pfr_hal pfr_game_flags pfr::SDL2main pfr::SDL2)
pfr_low_address_link(${PFR_EXE_NAME})
if(WIN32 AND CMAKE_C_COMPILER_LINKER_ID STREQUAL "LLD")   # lld-only option (all undefined symbols)
    target_link_options(${PFR_EXE_NAME} PRIVATE -Wl,--error-limit=0)
endif()

# End-to-end tests (need the generated assets), see scripts/smoke_test.py.
# Boot -> intro -> title -> new game -> Prof. Oak, intro music recorded.
add_test(NAME game_smoke
         COMMAND ${Python3_EXECUTABLE} ${PFR_ROOT}/src/platform_pc/scripts/smoke_test.py
                 $<TARGET_FILE:${PFR_EXE_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/smoke)
set_tests_properties(game_smoke PROPERTIES TIMEOUT 600)

# New game -> bedroom -> 1F -> Pallet Town -> a wild battle -> back.
add_test(NAME game_world
         COMMAND ${Python3_EXECUTABLE} ${PFR_ROOT}/src/platform_pc/scripts/smoke_test.py --world
                 $<TARGET_FILE:${PFR_EXE_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/world)
set_tests_properties(game_world PROPERTIES TIMEOUT 900)

# Menus (bag, trainer card), save, then continue the save (party, summary).
add_test(NAME game_save
         COMMAND ${Python3_EXECUTABLE} ${PFR_ROOT}/src/platform_pc/scripts/smoke_test.py --save
                 $<TARGET_FILE:${PFR_EXE_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/savetest)
set_tests_properties(game_save PROPERTIES TIMEOUT 900)

# ======================================================================
# T5Jool: Linux x86_64 and Android arm64 (clang, SDL2, DXVK-native).
#
# Entered from the top-level CMakeLists.txt on Linux and Android. The ${BIN_NAME}
# target already exists with the full Windows source list; like cmake/switch.cmake
# this swaps the platform sources: the portable half of the Switch layer
# (src/nx: Win32 over POSIX, sockets, sound mixer, the D3D9 bridge) plus
# T5Jool's own (src/t5: entry point, SDL windows and input).
#
#   T5_SP=ON   T5Jool      Campaign and Zombies (KISAK_SP)
#   T5_SP=OFF  T5Jool-MP   Multiplayer (KISAK_MP)
#
# Dependencies: SDL2, DXVK-native (-DDXVK_NATIVE_PREFIX=<install prefix>),
# FFmpeg (libavcodec, libavutil, libswresample), zstd.
# ======================================================================

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "T5Jool builds with clang")
endif()

option(T5_SP "Build the Campaign / Zombies executable (KISAK_SP); OFF: Multiplayer" ON)
set(DXVK_NATIVE_PREFIX "" CACHE PATH "DXVK-native install prefix (include/dxvk, lib/libdxvk_d3d9.so)")
if(NOT DXVK_NATIVE_PREFIX)
    message(FATAL_ERROR "Pass -DDXVK_NATIVE_PREFIX=<dir>: DXVK-native's d3d9.h and libdxvk_d3d9")
endif()

set(NX_DIR "${SRC_DIR}/nx")
set(T5_DIR "${SRC_DIR}/t5")

# ----- Sources: as on Switch, the Windows-only drivers out, the platform layer in -----
get_target_property(T5_SOURCES ${BIN_NAME} SOURCES)
list(REMOVE_ITEM T5_SOURCES
    ${TRACY_FILES}
    "${SRC_DIR}/win32/win_mini_dumper.cpp"
    "${SRC_DIR}/win32/win_splash.cpp"
    "${SRC_DIR}/win32/win_syscon.cpp"
    "${SRC_DIR}/win32/win_voice.cpp"
    "${SRC_DIR}/sound/snd_driver_xaudio2.cpp"
    "${SRC_DIR}/sound/snd_driver_xaudio2_dsp.cpp"
    "${SRC_DIR}/groupvoice/play_dsound.cpp"
    "${SRC_DIR}/groupvoice/record_dsound.cpp"
    "${SRC_DIR}/vpx/vpx.cpp"
)
list(FILTER T5_SOURCES EXCLUDE REGEX "/src/tracy/")
list(APPEND T5_SOURCES
    "${NX_DIR}/nx_wincompat.cpp"
    "${NX_DIR}/nx_winsock.cpp"
    "${NX_DIR}/nx_platform_stubs.cpp"
    "${NX_DIR}/nx_prof.cpp"
    "${NX_DIR}/nx_kbz_sys.cpp"
    "${NX_DIR}/nx_snd.cpp"
    "${T5_DIR}/t5_main.cpp"
    "${T5_DIR}/t5_platform.cpp"
    "${T5_DIR}/sdl/t5_user32.cpp"
    "${T5_DIR}/sdl/t5_xinput.cpp"
)
set_target_properties(${BIN_NAME} PROPERTIES SOURCES "${T5_SOURCES}")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(OPENBLOPS_SSE_EMULATION ON)   # the engine's SSE intrinsics through src/compat/sse
endif()
include("${CMAKE_CURRENT_LIST_DIR}/engine64.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/bink-ffmpeg.cmake")

if(T5_SP)
    set(T5_GAME_MODE KISAK_SP)
    set(T5_OUTPUT_NAME T5Jool)
else()
    set(T5_GAME_MODE KISAK_MP)
    set(T5_OUTPUT_NAME T5Jool-MP)
endif()
set_target_properties(${BIN_NAME} PROPERTIES OUTPUT_NAME ${T5_OUTPUT_NAME})

target_compile_definitions(${BIN_NAME} PUBLIC
    ${T5_GAME_MODE}
    KISAK_NX            # the portable 64-bit handheld build of OpenBLOPS-NX...
    T5_POSIX            # ...on Linux / Android instead of Horizon
    NX_DXVK
    OPENBLOPS_OFFLINE_MENUS
    _CRT_SECURE_NO_WARNINGS
    LTC_NO_ASM
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

target_include_directories(${BIN_NAME} BEFORE PUBLIC "${T5_DIR}/horizon" "${NX_DIR}/compat")
target_include_directories(${BIN_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SRC_DIR}"
    "${SRC_DIR}/libs"
    "${SRC_DIR}/libs/libtomcrypt-1.17/src/headers"
    "${SRC_DIR}/libs/libtommath-1.0"
    "${SRC_DIR}/libs/libvpx-1.5.0/include"
)

# -fsigned-char: MSVC chars are signed (ARM's default is unsigned); the decompiled code
# type-puns (no strict aliasing) and relies on wrapping overflow. The prefix header maps the
# MSVC keywords and intrinsics, as on Switch.
target_compile_options(${BIN_NAME} PRIVATE
    -fsigned-char -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
    -ffunction-sections -fdata-sections
    "SHELL:-include \"${NX_DIR}/compat/nx_prefix.h\""
    -w -Wno-c++11-narrowing
    $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-function-pointer-types>
)

# Windows-era paths ('\', mixed case) go through the case-insensitive normalizer (nx_wincompat.cpp)
target_link_options(${BIN_NAME} PRIVATE
    -Wl,--wrap=fopen -Wl,--wrap=remove -Wl,--wrap=rename -Wl,--gc-sections)

# ----- Direct3D 9: the engine's facade (src/nx/compat/d3d9.h) forwarded into DXVK-native -----
set(T5_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/t5-generated")
file(MAKE_DIRECTORY "${T5_GENERATED}")
file(WRITE "${T5_GENERATED}/nx_renderer_build.h"
    "#define NX_RENDERER_LABEL \"DXVK-native\"\n#define NX_RENDERER_CACHE \"dxvk\"\n#define NX_RENDERER_CONFIG \"dxvk.conf\"\n#define NX_MESA_VERSION \"system\"\n")
find_program(T5_PYTHON python3 REQUIRED)
execute_process(COMMAND "${T5_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/nx/gen-d3d9-bridge.py"
    "${NX_DIR}/compat/d3d9.h" "${T5_GENERATED}/d3d9_forwarders.inc" COMMAND_ERROR_IS_FATAL ANY)
add_library(t5_d3d9_bridge STATIC "${NX_DIR}/dxvk/d3d9_bridge.cpp")
target_compile_definitions(t5_d3d9_bridge PRIVATE NX_DXVK T5_POSIX)
target_include_directories(t5_d3d9_bridge PRIVATE
    "${T5_GENERATED}" "${T5_DIR}/horizon"
    "${DXVK_NATIVE_PREFIX}/include/dxvk" "${DXVK_NATIVE_PREFIX}/include/dxvk/wsi")
target_compile_options(t5_d3d9_bridge PRIVATE -O2 -fno-strict-aliasing -w)
find_library(T5_DXVK_D3D9 NAMES dxvk_d3d9 PATHS "${DXVK_NATIVE_PREFIX}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
target_link_libraries(t5_d3d9_bridge PUBLIC ${T5_DXVK_D3D9})

# ----- Libraries -----
find_package(PkgConfig REQUIRED)
pkg_check_modules(T5_FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil libswresample)
pkg_check_modules(T5_ZSTD REQUIRED IMPORTED_TARGET libzstd)
find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(${BIN_NAME} PRIVATE
    t5_d3d9_bridge SDL2::SDL2 PkgConfig::T5_FFMPEG PkgConfig::T5_ZSTD Threads::Threads ${CMAKE_DL_LIBS} m)

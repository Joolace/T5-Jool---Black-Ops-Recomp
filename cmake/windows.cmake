# ======================================================================
# Windows build configuration (MSVC). The architecture comes from the
# compiler: x86 is the original engine on the PC fastfiles and SDKs; ARM64
# and x64 build the 64-bit engine (cmake/engine64.cmake).
# ======================================================================

string(TOUPPER "${MSVC_CXX_ARCHITECTURE_ID}" OPENBLOPS_WIN_ARCH)
if(OPENBLOPS_WIN_ARCH STREQUAL "X86")
    set(OPENBLOPS_WIN_ARCH x86)
elseif(NOT OPENBLOPS_WIN_ARCH STREQUAL "ARM64" AND NOT OPENBLOPS_WIN_ARCH STREQUAL "X64")
    message(FATAL_ERROR "Unsupported Windows architecture '${MSVC_CXX_ARCHITECTURE_ID}'")
endif()

option(OPENBLOPS_OFFLINE_MENUS "Allow local play through the offline menu overrides" ON)
target_compile_definitions(${BIN_NAME} PRIVATE $<$<BOOL:${OPENBLOPS_OFFLINE_MENUS}>:OPENBLOPS_OFFLINE_MENUS>)

option(KISAK_SP "Build the single-player / Zombies executable" OFF)
if(KISAK_SP)
    set(KISAK_GAME_MODE KISAK_SP)
    set(OPENBLOPS_PC_NAME OpenBLOPS)
else()
    set(KISAK_GAME_MODE KISAK_MP)
    set(OPENBLOPS_PC_NAME OpenBLOPS-MP)
endif()

if(OPENBLOPS_WIN_ARCH STREQUAL "x86")
    set(OPENBLOPS_PC_OUTPUT_DEFAULT "${CMAKE_SOURCE_DIR}/build-nx/PC-build")
else()
    set(OPENBLOPS_PC_OUTPUT_DEFAULT "${CMAKE_SOURCE_DIR}/build-nx/PC-${OPENBLOPS_WIN_ARCH}-build")
endif()
set(OPENBLOPS_PC_OUTPUT_DIR "${OPENBLOPS_PC_OUTPUT_DEFAULT}"
    CACHE PATH "Directory for the Windows executables and symbols")

# ----- Preprocessor Defines -----
target_compile_definitions(${BIN_NAME} PUBLIC
    WIN32
    _CONSOLE
    ${KISAK_GAME_MODE}
    _CRT_SECURE_NO_WARNINGS
    _WINSOCK_NO_DEPRECATED_NO_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# ----- Include directories -----
target_include_directories(${BIN_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SRC_DIR}"
    "${SRC_DIR}/libs"
    "${SRC_DIR}/libs/libtomcrypt-1.17/src/headers"
    "${SRC_DIR}/libs/libtommath-1.0"
    "${SRC_DIR}/libs/libvpx-1.5.0/include"
)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT  /O2 /MP /W3 ${MSVC_WARNING_DISABLES} /permissive")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} /MTd /Od /MP /W3 ${MSVC_WARNING_DISABLES} /permissive")

# The decompiled code needs permissive mode, which -std:c++20 turns off;
# target options come after the standard flag on the command line.
target_compile_options(${BIN_NAME} PRIVATE /permissive)

if(NOT MSVC_VERSION VERSION_LESS 142)
    target_link_options(${BIN_NAME} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL>)
    target_compile_options(${BIN_NAME} PRIVATE $<$<CONFIG:Debug>:/ZI>)
endif()

# ----- Libraries common to every architecture -----
target_link_libraries(${BIN_NAME} PUBLIC
    d3d9.lib
    ws2_32.lib
    advapi32.lib
    dxguid.lib
    dsound.lib
    winmm.lib
    xinput.lib
    xapobase.lib
)

if(OPENBLOPS_WIN_ARCH STREQUAL "x86")
    if(DEFINED ENV{DXSDK_DIR})
        set(DXSDK_DIR "$ENV{DXSDK_DIR}" CACHE PATH "DirectX SDK directory")
    else()
        set(DXSDK_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)" CACHE PATH "DirectX SDK directory")
    endif()

    # phys_gjk.cpp specific flag (experimental)
    set_source_files_properties(
        "${SRC_DIR}/physics/phys_gjk.cpp"
        PROPERTIES
        COMPILE_OPTIONS "/arch:IA32"
    )

    target_compile_definitions(${BIN_NAME} PUBLIC
        $<$<CONFIG:Debug>:TRACY_ENABLE>
        $<$<CONFIG:Debug>:TRACY_ON_DEMAND>
    )
    target_include_directories(${BIN_NAME} PUBLIC "${DXSDK_DIR}/Include")
    target_link_directories(${BIN_NAME} PUBLIC
        "${DXSDK_DIR}/Lib/x86"
        "${SRC_DIR}/libs/libvpx-1.5.0"
        "${SRC_DIR}/steam/redistributable_bin"
        "${SRC_DIR}/binklib"
        "${SRC_DIR}/nvapi"
    )

    set(CMAKE_CXX_FLAGS_DEBUG           "${CMAKE_CXX_FLAGS_DEBUG} /SAFESEH:NO")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")

    target_link_libraries(${BIN_NAME} PUBLIC
        steam_api.lib
        binkw32.lib
        nvapi.lib
        dxerr.lib
        libvpx.lib
        # June-2010 DirectX SDK libs (xapobase, dxerr) reference pre-VS2015 CRT
        # symbols (_vsnprintf, etc.) that the modern UCRT no longer exports.
        # legacy_stdio_definitions.lib provides shims so those static libs link.
        legacy_stdio_definitions.lib
        $<$<CONFIG:Debug>:d3dx9d.lib>
        $<$<CONFIG:Release>:d3dx9.lib>
    )

    # /DYNAMICBASE:NO - Turning this off causes directx to crash "directx has encountered an unrecoverable error".
    # /LARGEADDRESSAWARE - for convenience (4gb)
    target_link_options(${BIN_NAME} PRIVATE /machine:x86 /DYNAMICBASE:NO /LARGEADDRESSAWARE)
else()
    if(OPENBLOPS_WIN_ARCH STREQUAL "ARM64")
        set(OPENBLOPS_SSE_EMULATION ON)
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/engine64.cmake")

    get_target_property(OPENBLOPS_WIN64_SOURCES ${BIN_NAME} SOURCES)
    list(REMOVE_ITEM OPENBLOPS_WIN64_SOURCES
        ${TRACY_FILES}                     # profiler: x86-only, Debug-only
        "${SRC_DIR}/vpx/vpx.cpp"           # VP8 clip encoder: libvpx ships for x86 only
    )
    set_target_properties(${BIN_NAME} PROPERTIES SOURCES "${OPENBLOPS_WIN64_SOURCES}")

    target_sources(${BIN_NAME} PRIVATE
        "${SRC_DIR}/win32/win_kbz.cpp"                       # Sys_Kbz* hooks for the zone loader
        "${SRC_DIR}/win32/win_bink.cpp"                      # movie clock
        "${SRC_DIR}/sound/snd_driver_xaudio2_cinematic.cpp"  # movie audio voice
    )

    # The Windows SDK replaces the June 2010 DirectX SDK: XAudio2 2.9 instead
    # of 2.7, and no D3DX or dxerr libraries (see src/compat).
    target_link_libraries(${BIN_NAME} PUBLIC
        xaudio2.lib
        ddraw.lib
    )

    # Bink movies through a shared FFmpeg build (include/, lib/*.lib, bin/*.dll),
    # e.g. https://github.com/tordona/ffmpeg-win-arm64 for ARM64. bink.h imports
    # the API from binkw32.dll unless it is implemented in the executable.
    include("${CMAKE_CURRENT_LIST_DIR}/bink-ffmpeg.cmake")
    target_compile_definitions(${BIN_NAME} PUBLIC __RADINEXE__)
    string(TOLOWER "${OPENBLOPS_WIN_ARCH}" OPENBLOPS_WIN_ARCH_LOWER)
    set(OPENBLOPS_FFMPEG_DIR "${CMAKE_SOURCE_DIR}/build-nx/deps/ffmpeg-win-${OPENBLOPS_WIN_ARCH_LOWER}"
        CACHE PATH "Shared FFmpeg build for this architecture")
    if(NOT EXISTS "${OPENBLOPS_FFMPEG_DIR}/include/libavcodec/avcodec.h")
        message(FATAL_ERROR "FFmpeg not found in OPENBLOPS_FFMPEG_DIR='${OPENBLOPS_FFMPEG_DIR}'")
    endif()
    target_include_directories(${BIN_NAME} PRIVATE "${OPENBLOPS_FFMPEG_DIR}/include")
    target_link_libraries(${BIN_NAME} PRIVATE
        "${OPENBLOPS_FFMPEG_DIR}/lib/avcodec.lib"
        "${OPENBLOPS_FFMPEG_DIR}/lib/swresample.lib"
        "${OPENBLOPS_FFMPEG_DIR}/lib/avutil.lib"
    )
    file(GLOB OPENBLOPS_FFMPEG_DLLS
        "${OPENBLOPS_FFMPEG_DIR}/bin/avcodec-*.dll"
        "${OPENBLOPS_FFMPEG_DIR}/bin/swresample-*.dll"
        "${OPENBLOPS_FFMPEG_DIR}/bin/avutil-*.dll")
    add_custom_command(TARGET ${BIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENBLOPS_FFMPEG_DLLS} "${OPENBLOPS_PC_OUTPUT_DIR}")

    # zstd decompresses .kbz zones; only its decoder is built.
    set(OPENBLOPS_ZSTD_DIR "${CMAKE_SOURCE_DIR}/build-nx/deps/zstd"
        CACHE PATH "zstd source tree (https://github.com/facebook/zstd, v1.5.7)")
    file(GLOB OPENBLOPS_ZSTD_SOURCES
        "${OPENBLOPS_ZSTD_DIR}/lib/common/*.c"
        "${OPENBLOPS_ZSTD_DIR}/lib/decompress/*.c")
    if(NOT OPENBLOPS_ZSTD_SOURCES)
        message(FATAL_ERROR "zstd sources not found in OPENBLOPS_ZSTD_DIR='${OPENBLOPS_ZSTD_DIR}'")
    endif()
    add_library(openblops_zstd STATIC ${OPENBLOPS_ZSTD_SOURCES})
    target_include_directories(openblops_zstd PUBLIC "${OPENBLOPS_ZSTD_DIR}/lib")
    target_compile_definitions(openblops_zstd PRIVATE ZSTD_DISABLE_ASM)
    target_link_libraries(${BIN_NAME} PRIVATE openblops_zstd)
endif()

# /DEBUG - PDB for both configs
target_link_options(${BIN_NAME} PRIVATE /DEBUG)

set_target_properties(${BIN_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

# Set the VS debugger's working directory to the build-output dir for whichever config is active
set_target_properties(${BIN_NAME} PROPERTIES
    OUTPUT_NAME "${OPENBLOPS_PC_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "$<1:${OPENBLOPS_PC_OUTPUT_DIR}>"
    PDB_OUTPUT_DIRECTORY "$<1:${OPENBLOPS_PC_OUTPUT_DIR}>"
    VS_DEBUGGER_WORKING_DIRECTORY "${OPENBLOPS_PC_OUTPUT_DIR}"
)

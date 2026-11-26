# ======================================================================
# Bink movies decoded with FFmpeg in place of the x86-only binkw32 library.
# The including platform links FFmpeg and implements the hooks declared in
# src/binklib/bink_audio.h.
# ======================================================================

get_target_property(OPENBLOPS_BINK_SOURCES ${BIN_NAME} SOURCES)
list(REMOVE_ITEM OPENBLOPS_BINK_SOURCES
    "${SRC_DIR}/binklib/dx9rad3d.cpp"      # RAD3D-over-D3D9 helper for binkw32
    "${SRC_DIR}/binklib/binktextures.cpp"  # binkw32 texture uploads; r_cinematic uploads planes itself
)
set_target_properties(${BIN_NAME} PROPERTIES SOURCES "${OPENBLOPS_BINK_SOURCES}")

target_sources(${BIN_NAME} PRIVATE
    "${SRC_DIR}/binklib/bink_ffmpeg.cpp"
    "${SRC_DIR}/binklib/bink_audio.cpp"
)
target_compile_definitions(${BIN_NAME} PUBLIC KISAK_BINK_FFMPEG)

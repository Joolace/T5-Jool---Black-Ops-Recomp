# ======================================================================
# Shared configuration for 64-bit builds, included by the platform modules.
# They load zones converted to the 64-bit layout (.kbz) and replace the
# x86-only SDKs with src/compat. The platform supplies zstd and the Sys_Kbz*
# hooks; OPENBLOPS_SSE_EMULATION adds src/compat/sse for CPUs without SSE.
# ======================================================================

target_compile_definitions(${BIN_NAME} PUBLIC KISAK_64BIT)

target_sources(${BIN_NAME} PRIVATE
    "${SRC_DIR}/database/db_kbz.cpp"
    "${SRC_DIR}/database/db_kbz_storage.cpp"
    "${SRC_DIR}/compat/compat_d3dx9.cpp"
    "${SRC_DIR}/compat/compat_sdk.cpp"
)

# The replacement SDK headers take priority over the x86 SDK copies in src/.
target_include_directories(${BIN_NAME} BEFORE PUBLIC "${SRC_DIR}/compat")

if(OPENBLOPS_SSE_EMULATION)
    target_include_directories(${BIN_NAME} BEFORE PUBLIC "${SRC_DIR}/compat/sse")
endif()

#!/usr/bin/env bash
# Builds the Switch 2 executables, SP and MP, into build-nx/NX2-build. Run under
# devkitPro MSYS2 with the ounce-devkit installed in $DEVKITPRO (libounce and
# its CMake toolchain; https://github.com/ouncebrew/ounce-devkit).
# The renderer is the Switch 1 build's native DXVK Sarek (tools/nx/build-sarek.sh)
# over Mesa NVK from the Switch 2 Horizon Mesa 26.2.7 SDK, unpacked in
# build-nx/deps/mesa-26.2.7-switch2; DXVK Sarek and its COM headers must be
# prepared in build-nx/deps too (prepare-sarek.py, prepare-dxvk.py).
# ICON=<256x256 JPEG> gives the NROs a homebrew menu icon.
set -euo pipefail
cd "$(dirname "$0")/../.."
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$DEVKITPRO/tools/bin:/ucrt64/bin:/usr/bin:$PATH"
toolchain="$DEVKITPRO/libounce/cmake/Switch2.cmake"
if [[ ! -f "$toolchain" ]]; then
  echo "No Switch 2 devkit in $DEVKITPRO: install https://github.com/ouncebrew/ounce-devkit" >&2
  exit 1
fi
if [[ ! -f build-nx/deps/mesa-26.2.7-switch2/opt/devkitpro/portlibs/switch2/lib/libmesa_vulkan.a ]]; then
  echo "No Switch 2 Mesa SDK: unzip mesa-26.2.7-switch2-horizon-sdk.zip into build-nx/deps/mesa-26.2.7-switch2" >&2
  exit 1
fi
jobs=${JOBS:-6}
output="$PWD/build-nx/NX2-build"
native="$output/renderer"
cmake -S tools/nx/dxvk -B "$native" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
  -DCMAKE_BUILD_TYPE=Release \
  -DNX_DXVK_SOURCE="$PWD/build-nx/deps/dxvk-sarek-1.14.1"
cmake --build "$native" -j "$jobs"
for mode in sp mp; do
  sp=OFF
  if [[ "$mode" == sp ]]; then sp=ON; fi
  build="$output/$mode"
  cmake -S . -B "$build" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
    -DCMAKE_BUILD_TYPE=Release -DNX_SP="$sp" -DNX_DXVK=ON \
    -DKISAK_DIRECT_CONNECT=ON -DOPENBLOPS_NX_OUTPUT_DIR="$output" \
    -DNX_DXVK_PREBUILT_DIR="$native" -DNX_ICON="${ICON:-}"
  cmake --build "$build" -j "$jobs"
done

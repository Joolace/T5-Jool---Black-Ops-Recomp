#!/bin/bash
# tools/t5/build_linux.sh - builds T5Jool for Linux x86_64 on the OpenBLOPS-NX base:
#   build-linux-sp/T5Jool      Campaign and Zombies
#   build-linux-mp/T5Jool-MP   Multiplayer
#   build-tools/convert, kbzdump   zone converter (.ff -> .kbz, needed once per install)
# DXVK-native comes from the T5Jool 0.8 build (its build-linux64 cache) or DXVK_NATIVE_PREFIX.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"

OLD=${T5_OLD_TREE:-$HOME/bo1-zombies-decompiled-main}
DXVK=${DXVK_NATIVE_PREFIX:-}
if [ -z "$DXVK" ] && [ -f "$OLD/build-linux64/CMakeCache.txt" ]; then
    DXVK=$(sed -n 's/^DXVK_NATIVE_PREFIX:[A-Z]*=//p' "$OLD/build-linux64/CMakeCache.txt")
fi
if [ -z "$DXVK" ] || [ ! -d "$DXVK" ]; then
    echo "DXVK-native not found: set DXVK_NATIVE_PREFIX=<dir with include/dxvk and lib/libdxvk_d3d9.so>"
    exit 1
fi
echo "=== DXVK-native: $DXVK"

missing=""
for pc in sdl2 libavcodec libavutil libswresample libzstd zlib; do
    pkg-config --exists "$pc" || missing="$missing $pc"
done
if [ -n "$missing" ]; then
    echo "Missing libraries:$missing"
    echo "Install them with:"
    echo "  sudo apt install libsdl2-dev libavcodec-dev libavutil-dev libswresample-dev libzstd-dev zlib1g-dev"
    exit 1
fi

for mode in sp mp; do
    SP=ON; [ $mode = mp ] && SP=OFF
    echo "=== $( [ $mode = sp ] && echo 'Campaign and Zombies' || echo 'Multiplayer' )"
    if [ ! -f build-linux-$mode/build.ninja ]; then
        cmake -G Ninja -S . -B build-linux-$mode -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
            -DT5_SP=$SP -DKISAK_DIRECT_CONNECT=ON -DDXVK_NATIVE_PREFIX="$DXVK" > /dev/null
    fi
    LC_ALL=C ninja -C build-linux-$mode 2>&1 | grep -E "error|FAILED|undefined|multiple definition|ld:|Linking" | grep -v "^ninja: no work" || true
    [ -x build-linux-$mode/T5Jool$( [ $mode = mp ] && echo -MP ) ] || { echo "Build failed (see above)"; exit 1; }
done

echo "=== Zone converter"
mkdir -p build-tools
g++ -O2 -std=c++17 -w tools/ffconv/convert.cpp -o build-tools/convert -lz
g++ -O2 -std=c++17 -w tools/ffconv/kbzdump.cpp -o build-tools/kbzdump
echo "=== Done: build-linux-sp/T5Jool, build-linux-mp/T5Jool-MP, build-tools/convert"

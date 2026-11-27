#!/bin/bash
# tools/t5/build_android.sh - builds the T5Jool 0.9 APK (arm64) on the OpenBLOPS-NX base.
#
# Uses the Android SDK/NDK, SDL2, DXVK, libadrenotools and lsfg-vk prepared by T5Jool 0.8's
# tools/android/setup.sh (in $T5_DEPS, default ~/t5jool-android). Adds, once, in $T5_DEPS/prefix-t5:
#   FFmpeg   WMA and Bink decoders + libswresample (sounds and cinematics)
#   zstd     static (compressed zones)
# Then: the engine twice (libmain.so Campaign/Zombies, libmain_mp.so Multiplayer), the zone converter
# (libt5convert.so, libt5kbzpack.so), every library into build-android/jnilibs, and the APK with Gradle.
#   tools/t5/build_android.sh            -> out/T5Jool-v<ver>-arm64-v8.2.apk
#   T5_ARMV80=1 tools/t5/build_android.sh -> generic arm64 build
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DEPS=${T5_DEPS:-$HOME/t5jool-android}
SDK=${ANDROID_HOME:-$DEPS/sdk}
NDK_VER=27.2.12479018
API=29
NDK=$SDK/ndk/$NDK_VER
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64
CMAKE=$SDK/cmake/3.22.1/bin/cmake
NINJA=$SDK/cmake/3.22.1/bin/ninja
CC=$TC/bin/aarch64-linux-android$API-clang
CXX=$TC/bin/aarch64-linux-android$API-clang++
PREFIX=$DEPS/prefix
EXTRA=$DEPS/prefix-t5
SRC=$DEPS/src
BUILD=$DEPS/build
JOBS=$(nproc)
OUT=$ROOT/build-android
JNI=$OUT/jnilibs/arm64-v8a
VER=$(sed -n 's/^#define T5JOOL_VERSION "\(.*\)"/\1/p' "$ROOT/src/t5jool_version.h")
step() { echo; echo "==== $* ===="; }

for f in "$PREFIX/lib/libSDL2.so" "$PREFIX/lib/libdxvk_d3d9.so" "$CXX"; do
    [ -e "$f" ] || { echo "Missing $f: run T5Jool 0.8's tools/android/setup.sh first"; exit 1; }
done
ANDROID_CMAKE=(-G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake"
               -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-$API -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release)

# ------------------------------------------------------------------------------ FFmpeg (WMA + Bink)
step "FFmpeg with the WMA and Bink decoders"
[ -d "$SRC/ffmpeg/.git" ] || git clone --depth 1 --branch n6.1.2 https://github.com/FFmpeg/FFmpeg.git "$SRC/ffmpeg"
if [ ! -f "$EXTRA/lib/libswresample.so" ]; then
    rm -rf "$BUILD/ffmpeg-t5" && mkdir -p "$BUILD/ffmpeg-t5"
    (cd "$BUILD/ffmpeg-t5" && "$SRC/ffmpeg/configure" --prefix="$EXTRA" --target-os=android --arch=aarch64 --cpu=armv8-a \
        --enable-cross-compile --cc="$CC" --cxx="$CXX" --ar="$TC/bin/llvm-ar" --nm="$TC/bin/llvm-nm" \
        --ranlib="$TC/bin/llvm-ranlib" --strip="$TC/bin/llvm-strip" --enable-pic --enable-shared --disable-static \
        --disable-programs --disable-doc --disable-everything \
        --enable-decoder=wmav1,wmav2,wmapro,bink,binkaudio_dct,binkaudio_rdft \
        --disable-avformat --disable-avdevice --disable-avfilter --disable-swscale --enable-swresample \
        --disable-postproc --disable-network)
    # names without the version number: only "lib*.so" files go in the APK
    sed -i 's/^SLIBNAME_WITH_VERSION=.*/SLIBNAME_WITH_VERSION=$(SLIBNAME)/; s/^SLIBNAME_WITH_MAJOR=.*/SLIBNAME_WITH_MAJOR=$(SLIBNAME)/; s/^SLIB_INSTALL_NAME=.*/SLIB_INSTALL_NAME=$(SLIBNAME)/; s/^SLIB_INSTALL_LINKS=.*/SLIB_INSTALL_LINKS=/' "$BUILD/ffmpeg-t5/ffbuild/config.mak"
    make -C "$BUILD/ffmpeg-t5" -j "$JOBS"
    make -C "$BUILD/ffmpeg-t5" install
fi

# --------------------------------------------------------------------------------------------- zstd
step "zstd"
[ -d "$SRC/zstd/.git" ] || git clone --depth 1 --branch v1.5.6 https://github.com/facebook/zstd.git "$SRC/zstd"
if [ ! -f "$EXTRA/lib/libzstd.a" ]; then
    "$CMAKE" -S "$SRC/zstd/build/cmake" -B "$BUILD/zstd" "${ANDROID_CMAKE[@]}" -DCMAKE_INSTALL_PREFIX="$EXTRA" \
        -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF > /dev/null
    "$CMAKE" --build "$BUILD/zstd" -j "$JOBS"
    "$CMAKE" --install "$BUILD/zstd" > /dev/null
fi

# ------------------------------------------------------------------------------------------- engine
V82=ON; [ -n "${T5_ARMV80:-}" ] && V82=OFF
for mode in sp mp; do
    SP=ON; [ $mode = mp ] && SP=OFF
    step "Engine: $( [ $mode = sp ] && echo 'Campaign and Zombies (libmain.so)' || echo 'Multiplayer (libmain_mp.so)' ), ARMv8.2 $V82"
    B=$OUT/build-$mode
    "$CMAKE" -S "$ROOT" -B "$B" "${ANDROID_CMAKE[@]}" -DT5_SP=$SP -DKISAK_DIRECT_CONNECT=ON -DT5_ARMV82=$V82 \
        -DT5_ANDROID_DEPS="$PREFIX" -DT5_ANDROID_EXTRA="$EXTRA" -DT5_ANDROID_SOURCES="$SRC" > /dev/null
    LC_ALL=C "$NINJA" -k 0 -C "$B" KisakBlack main_hook hook_impl file_redirect_hook gsl_alloc_hook 2>&1 \
        | grep -E "error|FAILED|undefined|multiple definition|ld:" | head -60 || true
    [ -f "$B/lib$( [ $mode = sp ] && echo main || echo main_mp ).so" ] || { echo "Engine build failed (see above)"; exit 1; }
done

# ---------------------------------------------------------------------------------- zone converter
step "Zone converter"
mkdir -p "$OUT/tools"
"$CXX" -O2 -std=c++17 -w -fPIE -pie -static-libstdc++ "$ROOT/tools/ffconv/convert.cpp" -o "$OUT/tools/libt5convert.so" -lz
"$CXX" -O2 -std=c++17 -w -fPIE -pie -static-libstdc++ -I"$EXTRA/include" "$ROOT/tools/ffconv/kbzpack.cpp" \
    "$EXTRA/lib/libzstd.a" -o "$OUT/tools/libt5kbzpack.so"

# ------------------------------------------------------------------------------------ jniLibs + APK
step "APK"
rm -rf "$OUT/jnilibs" && mkdir -p "$JNI"
STRIP=$TC/bin/llvm-strip
for f in "$OUT/build-sp/libmain.so" "$OUT/build-mp/libmain_mp.so"; do
    cp "$f" "$OUT/$(basename "$f" .so)-symbols.so"   # with symbols, for crash reports
    "$STRIP" --strip-debug -o "$JNI/$(basename "$f")" "$f"
done
find "$OUT/build-sp/libadrenotools" -name "lib*.so" -exec cp {} "$JNI/" \; 2>/dev/null || true
cp "$OUT/tools/libt5convert.so" "$OUT/tools/libt5kbzpack.so" "$JNI/"
cp -L "$PREFIX/lib/libSDL2.so" "$PREFIX/lib/libdxvk_d3d9.so" "$JNI/"
[ -f "$PREFIX/lib/liblsfg-vk.so" ] && cp -L "$PREFIX/lib/liblsfg-vk.so" "$JNI/"
cp -L "$EXTRA/lib/libavcodec.so" "$EXTRA/lib/libavutil.so" "$EXTRA/lib/libswresample.so" "$JNI/"
ls -la "$JNI"

# Gradle 8.9 needs Java 17-22
if [ -z "${JAVA_HOME:-}" ]; then
    JMAJ=$(java -version 2>&1 | sed -n 's/.*version "\([0-9]*\).*/\1/p' | head -1)
    if [ "${JMAJ:-0}" -gt 22 ]; then
        for j in /usr/lib/jvm/java-21-openjdk-* /usr/lib/jvm/java-17-openjdk-*; do
            [ -x "$j/bin/java" ] && { export JAVA_HOME="$j"; break; }
        done
    fi
fi
KEYSTORE=${T5_KEYSTORE:-$HOME/.t5jool/t5jool-release.jks}
if [ -f "$KEYSTORE" ] && [ -z "${T5_KEYSTORE_PASSWORD:-}" ]; then
    printf "Release key password (%s): " "$KEYSTORE" > /dev/tty
    read -r -s T5_KEYSTORE_PASSWORD < /dev/tty
    echo > /dev/tty
    export T5_KEYSTORE_PASSWORD
fi
export T5_DEPS=$DEPS ANDROID_HOME=$SDK
echo "sdk.dir=$SDK" > "$ROOT/android/local.properties"
chmod +x "$ROOT/android/gradlew"
(cd "$ROOT/android" && ./gradlew --no-daemon -q assembleRelease)
mkdir -p "$ROOT/out"
NAME=T5Jool-v$VER-arm64$( [ $V82 = ON ] && echo -v8.2 ).apk
cp "$ROOT/android/app/build/outputs/apk/release/app-release.apk" "$ROOT/out/$NAME"
echo
echo "APK: out/$NAME"
ADB=$SDK/platform-tools/adb
if [ "${1:-}" = "install" ] && "$ADB" get-state >/dev/null 2>&1; then
    "$ADB" install -r "$ROOT/out/$NAME" | tail -1
fi

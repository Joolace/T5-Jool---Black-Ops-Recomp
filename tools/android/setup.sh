#!/bin/bash
# tools/android/setup.sh - prepara tutto il necessario per compilare T5Jool per Android (arm64-v8a) su Ubuntu.
#
#   Android SDK + NDK (scaricati in $T5_DEPS/sdk, nessuna installazione di sistema)
#   SDL2           libSDL2.so + classi Java (stessa versione)
#   DXVK           libdxvk_d3d9.so (Direct3D 9 -> Vulkan), con la patch android/patches/dxvk-android.patch
#   libadrenotools sorgenti (li compila Gradle): driver Vulkan personalizzati come Turnip
#   FFmpeg         solo i decoder WMA (suoni xWMA del gioco), opzionale: T5_NO_FFMPEG=1 per saltarlo
#   lsfg-vk        frame generation di Lossless Scaling (liblsfg-vk.so), opzionale: T5_NO_LSFG=1 per saltarlo
#
# Uso:   tools/android/setup.sh            (la prima volta ~30-40 minuti, scarica ~3 GB)
# Poi:   tools/t5/build_android.sh         (compila l'APK)
# Cartella delle dipendenze: T5_DEPS (predefinita ~/t5jool-android). Rilanciarlo salta i passi gia' fatti.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DEPS=${T5_DEPS:-$HOME/t5jool-android}
SDK=${ANDROID_HOME:-$DEPS/sdk}
NDK_VER=27.2.12479018
API=29
PREFIX=$DEPS/prefix
SRC=$DEPS/src
BUILD=$DEPS/build
JNI=$PREFIX/jnilibs/arm64-v8a
SDL_TAG=${T5_SDL_TAG:-release-2.30.12}
DXVK_TAG=${T5_DXVK_TAG:-v2.5.3}
FFMPEG_TAG=${T5_FFMPEG_TAG:-n6.1.2}
JOBS=$(nproc)

step() { echo; echo "==== $* ===="; }

# ------------------------------------------------------------------------------------------- strumenti
missing=()
for c in git curl unzip java meson ninja pkg-config python3 make; do
    command -v "$c" >/dev/null 2>&1 || missing+=("$c")
done
command -v glslangValidator >/dev/null 2>&1 || command -v glslang >/dev/null 2>&1 || missing+=("glslang")
if [ ${#missing[@]} -gt 0 ]; then
    echo "Mancano: ${missing[*]}"
    echo "Installali con:"
    echo "  sudo apt install git curl unzip openjdk-17-jdk-headless meson ninja-build pkg-config python3 make glslang-tools"
    exit 1
fi
JAVA_MAJOR=$(java -version 2>&1 | sed -n 's/.*version "\([0-9]*\).*/\1/p' | head -1)
if [ "${JAVA_MAJOR:-0}" -lt 17 ]; then
    echo "Serve Java 17 o piu' recente (trovato: ${JAVA_MAJOR:-?}): sudo apt install openjdk-17-jdk-headless"
    exit 1
fi

mkdir -p "$DEPS" "$SRC" "$BUILD" "$JNI"

# ------------------------------------------------------------------------------------------- SDK e NDK
step "Android SDK e NDK in $SDK"
if [ ! -x "$SDK/cmdline-tools/latest/bin/sdkmanager" ]; then
    tmp=$(mktemp -d)
    curl -fL -o "$tmp/cmdline.zip" https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
    unzip -q "$tmp/cmdline.zip" -d "$tmp"
    mkdir -p "$SDK/cmdline-tools"
    rm -rf "$SDK/cmdline-tools/latest"
    mv "$tmp/cmdline-tools" "$SDK/cmdline-tools/latest"
    rm -rf "$tmp"
fi
SDKM="$SDK/cmdline-tools/latest/bin/sdkmanager --sdk_root=$SDK"
yes | $SDKM --licenses >/dev/null || true
$SDKM "platform-tools" "platforms;android-34" "build-tools;34.0.0" "ndk;$NDK_VER" "cmake;3.22.1"
NDK=$SDK/ndk/$NDK_VER
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64
CMAKE=$SDK/cmake/3.22.1/bin/cmake
NINJA=$SDK/cmake/3.22.1/bin/ninja
CC=$TC/bin/aarch64-linux-android$API-clang
CXX=$TC/bin/aarch64-linux-android$API-clang++
ANDROID_CMAKE=(-G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake"
               -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX")

clone() { # clone <url> <tag> <cartella> [opzioni git]
    local url=$1 tag=$2 dir=$3; shift 3
    if [ ! -d "$dir/.git" ]; then
        git clone --depth 1 --branch "$tag" "$@" "$url" "$dir"
    fi
}

# --------------------------------------------------------------------------------------------- SDL2
step "SDL2 $SDL_TAG"
clone https://github.com/libsdl-org/SDL.git "$SDL_TAG" "$SRC/SDL2"
if [ ! -f "$PREFIX/lib/libSDL2.so" ]; then
    "$CMAKE" -S "$SRC/SDL2" -B "$BUILD/SDL2" "${ANDROID_CMAKE[@]}" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF
    "$CMAKE" --build "$BUILD/SDL2" -j "$JOBS"
    "$CMAKE" --install "$BUILD/SDL2"
fi
cp -L "$PREFIX/lib/libSDL2.so" "$JNI/"

# ------------------------------------------------------------------------------------- libadrenotools
step "libadrenotools"
if [ ! -d "$SRC/libadrenotools/.git" ]; then
    git clone --recurse-submodules https://github.com/bylaws/libadrenotools.git "$SRC/libadrenotools"
fi

# --------------------------------------------------------------------------------------------- DXVK
step "DXVK $DXVK_TAG"
if [ ! -d "$SRC/dxvk/.git" ]; then
    git clone --depth 1 --branch "$DXVK_TAG" --recurse-submodules --shallow-submodules https://github.com/doitsujin/dxvk.git "$SRC/dxvk"
fi
# patch T5Jool: riapplicata (e DXVK ricompilato) quando cambia
if ! cmp -s "$ROOT/android/patches/dxvk-android.patch" "$SRC/dxvk/.t5jool.patch"; then
    (cd "$SRC/dxvk" && git checkout -- . && git apply "$ROOT/android/patches/dxvk-android.patch")
    cp "$ROOT/android/patches/dxvk-android.patch" "$SRC/dxvk/.t5jool.patch"
    rm -f "$PREFIX/lib/libdxvk_d3d9.so"
fi
cat > "$BUILD/android-aarch64.cross" <<EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$TC/bin/llvm-ar'
strip = '$TC/bin/llvm-strip'
pkg-config = 'pkg-config'

[built-in options]
cpp_link_args = ['-static-libstdc++']

[properties]
pkg_config_libdir = '$PREFIX/lib/pkgconfig'

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8'
endian = 'little'
EOF
if [ ! -f "$PREFIX/lib/libdxvk_d3d9.so" ]; then
    rm -rf "$BUILD/dxvk"
    meson setup "$BUILD/dxvk" "$SRC/dxvk" --cross-file "$BUILD/android-aarch64.cross" --prefix "$PREFIX" \
        --buildtype release -Dbuild_id=false -Denable_d3d8=false -Denable_d3d10=false -Denable_d3d11=false -Denable_dxgi=false
    ninja -C "$BUILD/dxvk" -j "$JOBS"
    ninja -C "$BUILD/dxvk" install
fi
cp -L "$PREFIX/lib/libdxvk_d3d9.so" "$JNI/"

# ------------------------------------------------------------------------------------------- FFmpeg
if [ "${T5_NO_FFMPEG:-0}" != "1" ]; then
    step "FFmpeg $FFMPEG_TAG (solo decoder WMA)"
    clone https://github.com/FFmpeg/FFmpeg.git "$FFMPEG_TAG" "$SRC/ffmpeg"
    if [ ! -f "$PREFIX/lib/libavcodec.so" ]; then
        rm -rf "$BUILD/ffmpeg" && mkdir -p "$BUILD/ffmpeg"
        (cd "$BUILD/ffmpeg" && "$SRC/ffmpeg/configure" --prefix="$PREFIX" --target-os=android --arch=aarch64 --cpu=armv8-a \
            --enable-cross-compile --cc="$CC" --cxx="$CXX" --ar="$TC/bin/llvm-ar" --nm="$TC/bin/llvm-nm" \
            --ranlib="$TC/bin/llvm-ranlib" --strip="$TC/bin/llvm-strip" --enable-pic --enable-shared --disable-static \
            --disable-programs --disable-doc --disable-everything --enable-decoder=wmav1,wmav2,wmapro \
            --disable-avformat --disable-avdevice --disable-avfilter --disable-swscale --disable-swresample \
            --disable-postproc --disable-network)
        # nomi senza numero di versione: nell'APK entrano solo file "lib*.so"
        sed -i 's/^SLIBNAME_WITH_VERSION=.*/SLIBNAME_WITH_VERSION=$(SLIBNAME)/; s/^SLIBNAME_WITH_MAJOR=.*/SLIBNAME_WITH_MAJOR=$(SLIBNAME)/; s/^SLIB_INSTALL_NAME=.*/SLIB_INSTALL_NAME=$(SLIBNAME)/; s/^SLIB_INSTALL_LINKS=.*/SLIB_INSTALL_LINKS=/' "$BUILD/ffmpeg/ffbuild/config.mak"
        make -C "$BUILD/ffmpeg" -j "$JOBS"
        make -C "$BUILD/ffmpeg" install
    fi
    cp -L "$PREFIX/lib/libavcodec.so" "$PREFIX/lib/libavutil.so" "$JNI/"
fi

# ------------------------------------------------------------------------- lsfg-vk (Lossless Scaling)
# Frame generation di Lossless Scaling: liblsfg-vk.so (ramo Android di GameNative, licenza MIT), caricata da DXVK
# con T5_LSFG=1 (patch dxvk-android.patch). Gli shader li prende a runtime dalla Lossless.dll dell'utente.
# T5_NO_LSFG=1 per saltarlo.
LSFG_COMMIT=${T5_LSFG_COMMIT:-437208aad4296629bc5ebd98570f932c612a69fa}
if [ "${T5_NO_LSFG:-0}" != "1" ]; then
    step "lsfg-vk ${LSFG_COMMIT:0:7} (frame generation di Lossless Scaling)"
    if [ ! -d "$SRC/lsfg-vk/.git" ]; then
        git clone https://github.com/GameNative/lsfg-vk-android.git "$SRC/lsfg-vk"
    fi
    if [ "$(git -C "$SRC/lsfg-vk" rev-parse HEAD)" != "$LSFG_COMMIT" ]; then
        git -C "$SRC/lsfg-vk" fetch origin
        git -C "$SRC/lsfg-vk" checkout -q "$LSFG_COMMIT"
        rm -f "$PREFIX/lib/liblsfg-vk.so"
    fi
    git -C "$SRC/lsfg-vk" submodule update --init --recursive
    # vk_layer.h (protocollo dei livelli Vulkan): non e' tra gli header del NDK.
    # VK_USE_PLATFORM_ANDROID_KHR: volk.h include allora vulkan_android.h (tipi AHardwareBuffer di framegen)
    clone https://github.com/KhronosGroup/Vulkan-Headers.git v1.4.313 "$SRC/Vulkan-Headers"
    if [ ! -f "$PREFIX/lib/liblsfg-vk.so" ]; then
        rm -rf "$BUILD/lsfg-vk"
        "$CMAKE" -S "$SRC/lsfg-vk" -B "$BUILD/lsfg-vk" "${ANDROID_CMAKE[@]}" \
            -DCMAKE_CXX_FLAGS="-I$SRC/Vulkan-Headers/include -DVK_USE_PLATFORM_ANDROID_KHR" \
            -DCMAKE_C_FLAGS="-I$SRC/Vulkan-Headers/include -DVK_USE_PLATFORM_ANDROID_KHR"
        "$CMAKE" --build "$BUILD/lsfg-vk" --target lsfg-vk -j "$JOBS"
        mkdir -p "$PREFIX/lib"
        cp "$BUILD/lsfg-vk/liblsfg-vk.so" "$PREFIX/lib/"
    fi
    cp -L "$PREFIX/lib/liblsfg-vk.so" "$JNI/"
fi

# ----------------------------------------------------------------------------------------- Gradle
echo "sdk.dir=$SDK" > "$ROOT/android/local.properties"
chmod +x "$ROOT/android/gradlew"

step "Fatto"
echo "Dipendenze in $DEPS"
ls -la "$JNI"
echo
echo "Ora: tools/android/build.sh"

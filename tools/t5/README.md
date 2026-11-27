# T5Jool on the OpenBLOPS-NX base

T5Jool 0.9 runs on [OpenBLOPS-NX](https://github.com/HDErick/OpenBLOPS-NX) (GPL-3.0): Campaign,
Zombies and Multiplayer, with a native 64-bit engine that loads zones converted once to the
64-bit layout (`.kbz`). T5Jool adds Linux and Android on top of it.

## Layout

| Folder | What |
|---|---|
| `src/nx` | the Switch layer. Its portable half (Win32 over POSIX, sockets, sound mixer, the D3D9 bridge) is shared with Linux and Android |
| `src/t5` | T5Jool: entry point, SDL windows / input / audio output, `horizon/switch.h` (the few libnx primitives the shared files use, on POSIX) |
| `cmake/t5jool.cmake` | the Linux / Android build |
| `tools/t5` | build and zone conversion scripts |

`T5_POSIX` marks the engine code that differs from the Switch build (mouse and keyboard, vsync).

## Linux

```sh
tools/t5/build_linux.sh                       # T5Jool, T5Jool-MP, the zone converter
tools/t5/convert_zones.sh ~/Games/BlackOps    # once: zone/*.ff -> kbz/*.kbz
build-linux-sp/T5Jool --t5-home=$HOME/Games/BlackOps      # Campaign and Zombies
build-linux-mp/T5Jool-MP --t5-home=$HOME/Games/BlackOps   # Multiplayer
```

Needs clang, cmake, ninja, SDL2, FFmpeg (libavcodec, libavutil, libswresample), zstd, zlib and
DXVK-native (`DXVK_NATIVE_PREFIX`, or the one from the T5Jool 0.8 build).

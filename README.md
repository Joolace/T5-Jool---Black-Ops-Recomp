# T5Jool - Black Ops Recomp

T5Jool is a native port of **Call of Duty: Black Ops (2010)** to **Linux** and **Android**.
Campaign, Zombies and Multiplayer run on a native 64-bit engine, with DXVK for the graphics
and SDL2 for windows, input and audio. No emulation, no Wine.

Since version 0.9, T5Jool is built on [OpenBLOPS-NX](https://github.com/HDErick/OpenBLOPS-NX),
the Black Ops source reconstruction for PC and Nintendo Switch by HDErick, NaGaa95 and the
OpenBLOPS team (see [Credits](#credits)). T5Jool adds the Linux and Android layer on top.

**You need your own copy of the PC game.** This repository contains source code and tools only:
no game files are included, and none are downloaded.

Discord: https://discord.gg/rcMeKZcShx

## Features

- Campaign, Zombies (base game and DLC maps you own) and Multiplayer
- Multiplayer: offline matches with bots, Host Match and the community server browser, direct connect
- Android launcher: game files check, graphics presets, custom Vulkan drivers (Turnip for Adreno,
  PanVK for Mali), resolution scale and upscalers, frame generation, logs and bug reports
- Touch controls in game and in menus, controllers, keyboard and mouse
- Zones are converted once to the 64-bit layout (`kbz/`), on the first start on Android

## Building

### Linux

```sh
tools/t5/build_linux.sh                       # T5Jool, T5Jool-MP and the zone converter
tools/t5/convert_zones.sh ~/Games/BlackOps    # once: zone/*.ff -> kbz/*.kbz
build-linux-sp/T5Jool --t5-home=$HOME/Games/BlackOps      # Campaign and Zombies
build-linux-mp/T5Jool-MP --t5-home=$HOME/Games/BlackOps   # Multiplayer
```

Needs clang, cmake, ninja, SDL2, FFmpeg (libavcodec, libavutil, libswresample), zstd, zlib and
DXVK-native (`DXVK_NATIVE_PREFIX`). The script prints the packages it is missing.

### Android (arm64)

```sh
tools/t5/build_android.sh            # builds out/T5Jool-v<version>-arm64.apk
tools/t5/build_android.sh install    # and installs it with adb
```

The first time, `tools/android/setup.sh` downloads and builds the Android SDK and NDK and the
native dependencies (SDL2, DXVK, adrenotools) in `~/t5jool-android` (`$T5_DEPS`). The build
script builds FFmpeg and zstd itself.

### Community server browser

The default directory address is not part of the source. Put it in `openblops.local.cmake`
(git-ignored) next to `CMakeLists.txt`:

```cmake
set(OPENBLOPS_DIRECTORY_DEFAULT "IPv4:port")
```

or start the game with `+set bo1_directory IPv4:port`.

More details on the layout of the code: [tools/t5/README.md](tools/t5/README.md).

## Credits

- **OpenBLOPS-NX** by [HDErick](https://github.com/HDErick) and [NaGaa95](https://github.com/NaGaa95/), the engine T5Jool runs on.
  Their credits ([README-OPENBLOPS.md](README-OPENBLOPS.md)):
  - Multiplayer implementation from SwagSoftware
  - Initial base-game Zombies implementation by [femboyvalve](https://github.com/femboyvalve)
  - Zombies DLC and the full Campaign implementation by [NaGaa95](https://github.com/NaGaa95/)
  - Installer idea and compression techniques by [Chansey](https://github.com/ChanseyIsTheBest), [xsaja](https://github.com/xsaja) and [StevensND](https://github.com/StevensND)
  - Server design and online functionality by [ToeKnee](https://github.com/tonytrawl)
- **T5Jool** (Linux and Android port, launcher, touch controls) by Joolace
- [DXVK](https://github.com/doitsujin/dxvk), [SDL2](https://www.libsdl.org), [FFmpeg](https://ffmpeg.org),
  [zstd](https://github.com/facebook/zstd), [libadrenotools](https://github.com/bylaws/libadrenotools)

T5Jool is a fan project, not affiliated with or endorsed by Activision or Treyarch.
Call of Duty and Black Ops are trademarks of Activision Publishing, Inc.

## License

[GPL-3.0](LICENSE), like OpenBLOPS-NX.

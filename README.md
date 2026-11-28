<p align="center">
  <img src="docs/assets/logo.png" width="260" alt="T5Jool">
</p>

<h1 align="center">T5JOOL</h1>
<p align="center"><b>Black Ops Recomp</b><br>
A native 64-bit port of Black Ops (Campaign, Zombies and Multiplayer) for Linux and Android</p>

<p align="center">
  <a href="https://github.com/Joolace/T5-Jool---Black-Ops-Recomp/releases"><img src="https://img.shields.io/badge/download-v0.11--alpha-e2242a?style=for-the-badge" alt="Download"></a>
  <a href="https://discord.gg/rcMeKZcShx"><img src="https://img.shields.io/badge/discord-join-5865F2?style=for-the-badge" alt="Discord"></a>
  <a href="https://joolace.github.io/T5Jool/"><img src="https://img.shields.io/badge/website-visit-111114?style=for-the-badge" alt="Website"></a>
</p>

---

<p align="center">
  <a href="https://youtu.be/CukgP1mmFNA"><img src="https://img.youtube.com/vi/CukgP1mmFNA/maxresdefault.jpg" width="720" alt="T5Jool video: Black Ops Zombies running natively on Android"></a><br>
  <sub>▶ Watch T5Jool in action on YouTube</sub>
</p>

> [!CAUTION]
> **Alpha software, still in development.** Expect bugs, crashes and missing features. Save often, keep your expectations low, and please report what you find.

> [!IMPORTANT]
> T5Jool does **not** include any game files. You need your own legal copy of *Call of Duty: Black Ops* for PC.

> [!NOTE]
> **New in v0.10: a new engine and the source code.** T5Jool is now built on [OpenBLOPS-NX](https://github.com/HDErick/OpenBLOPS-NX)
> by HDErick, NaGaa95 and the OpenBLOPS team (GPL-3.0), with the T5Jool Linux and Android layer on top.
> Some v0.8 features are not on the new engine yet (see [What's new](#whats-new)). The v0.8 code is in the
> [`legacy-0.8`](https://github.com/Joolace/T5-Jool---Black-Ops-Recomp/tree/legacy-0.8) branch.

## Features

- **Native, not emulated**: the game code runs as a 64-bit program on x86_64 Linux and arm64 Android
- **The whole game**: Campaign, Zombies (all the maps you own) and Multiplayer
- **Vulkan renderer**: Direct3D 9 translated by DXVK, with a persistent shader cache
- **Upscalers**: Snapdragon GSR and FSR 1, render scale and sharpness options
- **Graphics presets**: Potato, Battery, Balanced, Quality, Smooth
- **Frame generation**: Lossless Scaling frame generation, with your own copy of Lossless Scaling
- **FXAA** antialiasing on Android
- **Multiplayer**: offline matches with bots, **Host Match** and the community **server browser**, direct connect, a relay for routers and mobile networks
- **Field of view** up to 110 degrees, from the launcher or the game's options
- **Controls**: controllers, handheld buttons, mouse and keyboard, a touch layout you can move and resize, and an on-screen D-pad and pad buttons in every menu
- **Vibration and gyro aiming**: on the controller or the device itself
- **Mods**: community mods made with the Black Ops mod tools, chosen in the launcher (converted at the first start like the game's zones)
- **Custom Vulkan drivers**: Turnip on Adreno, PanVK on Mali (experimental)
- **Automatic updates**: the Android launcher updates T5Jool and your Turnip driver for you

## Supported platforms

| | Android | Linux |
|---|---|---|
| Architecture | arm64 | x86_64 |
| GPU | Adreno: Turnip driver recommended, stock Qualcomm driver works | Any Vulkan GPU |
| Tested on | AYN Odin 3 Pro | Acer Nitro V16 (Ubuntu, NVIDIA GPU) |

Mali GPUs: **experimental**. The stock Mali driver does not work: install a **PanVK** driver in the launcher (tested in game on a Mali-G615). Reports welcome!

### Tested setup

| | |
|---|---|
| Device | **AYN Odin 3 Pro**: Snapdragon 8 Elite, Adreno 830, Android 15 |
| GPU drivers | Turnip by [Mr Purple (purple-turnip)](https://github.com/MrPurple666/purple-turnip/releases) and the Turnip builds by [StevenMXZ](https://github.com/StevenMXZ/Adreno-Tools-Drivers/releases) (e.g. Turnip Gen8 V39) |
| Game language | **English**. Other languages should work but have been tested much less |
| Linux | Acer Nitro V16, Ubuntu, NVIDIA GPU |

Other devices may work but have not been tested yet: reports are welcome.

## Install (Android)

1. Copy your PC installation of Black Ops (the folder with `zone` and `main`) to the device.
2. Download the latest APK from [Releases](https://github.com/Joolace/T5-Jool---Black-Ops-Recomp/releases) and install it:
   - `T5Jool-vX-arm64-v8.2.apk`: **recommended**, optimized for Snapdragon 845 and newer
   - `T5Jool-vX-arm64.apk`: generic, for older 64-bit devices
3. **Install a Turnip driver** (recommended on Adreno): the first time you press Play the launcher offers to download one. Pick a driver from Mr Purple or StevenMXZ and it is installed automatically. You can change it later in Settings > Game, or import your own `.zip`. Or choose **Try system driver** to use the stock Qualcomm driver.
4. In Settings > Game choose the game folder.
5. Tap **Zombies** or **Multiplayer**. The first start converts the game's zones once to the 64-bit format (a few minutes, with a progress bar).

> [!NOTE]
> The stock Qualcomm Vulkan driver works from v0.5, but it is slower than Turnip and stutters while it compiles shaders
> (less and less as the shader cache fills). Turnip is still the best choice.

The first launch compiles shaders, so the first minutes can stutter a little.

## Multiplayer

- **Offline Match**: a match against bots
- **Host Match**: pick map and mode and press Start; the match appears in the community server browser
- **Server browser** (Unranked tab): matches hosted by other players. If a direct connection is not possible, the game goes through the directory's relay, so no port forwarding is needed

Zombies co-op online and the T5Jool dedicated servers of v0.8 ([SERVER.md](SERVER.md)) are not on the new engine yet.

## Frame generation and antialiasing

**Android:** Settings → **Antialiasing (post-process) → FXAA** smooths jagged edges at a low cost.

**Android frame generation (experimental):** Settings → **Frame generation**. You need your own copy of
[Lossless Scaling](https://store.steampowered.com/app/993090/Lossless_Scaling/) on Steam: copy `Lossless.dll` from
`steamapps/common/Lossless Scaling` to your device and import it once (it stays in the app's private storage and is never
shared). Then choose 2x, 3x or 4x. Works best with a stable frame rate (set an FPS limit) and adds a little input lag.

**Linux:** [lsfg-vk](https://github.com/PancakeTAS/lsfg-vk) (frame generation, with your own copy of Lossless Scaling)
and [vkBasalt](https://github.com/DadSchoorse/vkBasalt) (FXAA / SMAA) work as Vulkan layers on `T5Jool` and `T5Jool-MP`.

## Mods

Put a community mod (made with the Black Ops mod tools) in the game folder as `mods/<name>/` with its `mod.ff`,
then choose it in the launcher: **Settings > Game > Mods**. At the next start the launcher converts the mod's zones once,
like the game's, and the game starts with the mod. **None** goes back to the base game. To play a mod online, everyone
needs the same mod. On Linux run `tools/t5/convert_zones.sh` again, then start with `+set fs_game mods/<name>`.

## What's new

See the full [CHANGELOG](CHANGELOG.md). Highlights of **v0.11-alpha**:
- **Lower render resolutions** for entry-level phones, down to 240p
- Fixed: a crash while moving with the stick pushed diagonally
- Fixed: a crash when the game lost its window with a custom Vulkan driver (Turnip, PanVK)

**v0.10-alpha**:
- **New engine**: built on OpenBLOPS-NX, with the **Campaign** next to Zombies and Multiplayer
- **Source code released** under GPL-3.0
- **Multiplayer**: Offline Match, Host Match and the community server browser
- **Touch-only play**: on-screen D-pad and pad buttons in every menu, smoother touch aim
- **Mods are back**, field of view up to 110°, and every launcher setting works on the new engine
- Fixed: Summit freezing at match start, broken glass that never fell
- Not on the new engine yet: solo saves, Dead Ops Arcade, Zombies co-op online

**v0.8-alpha**:
- **No longer Zombies only**: new menu and launcher with **Zombies** and **Multiplayer** sections
- **Host online games**: Zombies co-op and Multiplayer matches (beta) that anyone can find, with UPnP or the T5Jool relay
- **Multiplayer (beta)**: matches against bots, Create a Class, touch controls and gamepads

**v0.7 / v0.7.1-experimental**:
- Fewer "Out of memory" errors on phones with less memory
- **Mali GPUs with PanVK** reach the game (experimental)
- Second fire button and D-pad in the touch controls, menus scroll with a finger
- Gamepad mappings with `gamecontrollerdb.txt`, no more stretched image on screens wider than 16:9

**v0.6-alpha**:
- **Faster save loading**: perks, weapons, power and doors come back almost at once (no more bottle animations)
- **Potato graphics preset** for weak PCs and devices: in game (Options > Graphics) and in the Android launcher
- Android: the launcher's graphics quality now really changes the texture detail

Previous version, **v0.5-alpha**:
- **Solo save** (experimental): saved at the start of every round, one save per map. **Play Solo > Load game**,
  **Retry round** after a game over, **Delete save**
- **Stock Qualcomm driver works**: Turnip is no longer required on Adreno (still faster)
- **New Android launcher**: new home screen, settings in categories with explanations, graphics presets, loading screen
  between maps
- Turnip's shader cache is now kept between games (faster loading), no more checkerboard square on Moon's HUD

Before that, **v0.4-alpha**: Lossless Scaling frame generation and FXAA on Android, online fixes.

## Reporting bugs

In the launcher use **Report a bug** (home screen) or Settings > Help and about to save the logs, then share them on [Discord](https://discord.gg/rcMeKZcShx) or in an issue.

## Roadmap

- [x] Native Linux and Android builds
- [x] Upscalers and resolution scaling
- [x] LAN co-op with discovery and join in progress
- [x] Rumble, gyro aiming, mouse and keyboard on Android
- [x] Automatic updates for the launcher and the GPU driver
- [ ] Mali GPU support (experimental with PanVK, needs testers)
- [x] Dedicated servers (Linux) and online server browser (v0.3)
- [x] Mods menu for community mods (v0.3)
- [x] Lossless Scaling frame generation and FXAA (v0.4)
- [x] Save and resume in solo (v0.5)
- [x] Stock Qualcomm driver on Adreno (v0.5)
- [x] Faster save loading and Potato preset (v0.6)
- [x] Multiplayer against bots and Create a Class (v0.7)
- [x] Host online games with UPnP and relay, Zombies and Multiplayer menus (v0.8)
- [ ] Bots in online multiplayer matches
- [ ] Seamless map change on servers (no reconnect)
- [ ] Custom maps and engine mods
- [ ] Fuller solo saves (mystery box, Pack-a-Punch links, equipment)
- [x] New engine (OpenBLOPS-NX), Campaign and source code release (v0.10)
- [x] Lower resolutions for entry-level phones (v0.11)
- [ ] v0.8 features on the new engine: solo saves, Zombies co-op online

## Source code

The source code is in this repository, under [GPL-3.0](LICENSE) like OpenBLOPS-NX.

### Building

#### Linux

```sh
tools/t5/build_linux.sh                       # T5Jool, T5Jool-MP and the zone converter
tools/t5/convert_zones.sh ~/Games/BlackOps    # once: zone/*.ff -> kbz/*.kbz
build-linux-sp/T5Jool --t5-home=$HOME/Games/BlackOps      # Campaign and Zombies
build-linux-mp/T5Jool-MP --t5-home=$HOME/Games/BlackOps   # Multiplayer
```

Needs clang, cmake, ninja, SDL2, FFmpeg (libavcodec, libavutil, libswresample), zstd, zlib and
DXVK-native (`DXVK_NATIVE_PREFIX`). The script prints the packages it is missing.

#### Android (arm64)

```sh
tools/t5/build_android.sh            # builds out/T5Jool-v<version>-arm64.apk
tools/t5/build_android.sh install    # and installs it with adb
```

The first time, `tools/android/setup.sh` downloads and builds the Android SDK and NDK and the
native dependencies (SDL2, DXVK, adrenotools) in `~/t5jool-android` (`$T5_DEPS`). The build
script builds FFmpeg and zstd itself.

#### Community server browser

The default directory address is not part of the source. Put it in `openblops.local.cmake`
(git-ignored) next to `CMakeLists.txt`:

```cmake
set(OPENBLOPS_DIRECTORY_DEFAULT "IPv4:port")
```

or start the game with `+set bo1_directory IPv4:port`.

More details on the layout of the code: [tools/t5/README.md](tools/t5/README.md).

## Credits

- **Joolace**: development
- **[OpenBLOPS-NX](https://github.com/HDErick/OpenBLOPS-NX)** by [HDErick](https://github.com/HDErick), [NaGaa95](https://github.com/NaGaa95/) and the OpenBLOPS team: the engine T5Jool runs on since v0.10 (their credits: [README-OPENBLOPS.md](README-OPENBLOPS.md)): Multiplayer from SwagSoftware, base-game Zombies by [femboyvalve](https://github.com/femboyvalve), Zombies DLC and Campaign by NaGaa95, installer and compression by [Chansey](https://github.com/ChanseyIsTheBest), [xsaja](https://github.com/xsaja) and [StevensND](https://github.com/StevensND), servers and online by [ToeKnee](https://github.com/tonytrawl)
- **MisaDev4**: [bo1-zombies-decompiled](https://github.com/MisaDev4/bo1-zombies-decompiled), the base of T5Jool up to v0.8
- **iloupv**: contributions
- **[micycle1](https://github.com/micycle1/bo1-zombies-decompiled)**: engine fixes (lighting, net fields, decompile bugs)
- **[Mr Purple](https://github.com/MrPurple666/purple-turnip)** and **[StevenMXZ](https://github.com/StevenMXZ/Adreno-Tools-Drivers)**: the Turnip drivers T5Jool is tested with
- [lsfg-vk](https://github.com/PancakeTAS/lsfg-vk) by PancakeTAS and its Android branch by [GameNative](https://github.com/GameNative/lsfg-vk-android) (MIT): frame generation. Lossless Scaling itself is a separate paid app and is not included
- [SDL2](https://www.libsdl.org/), [FFmpeg](https://ffmpeg.org), [zstd](https://github.com/facebook/zstd), [libadrenotools](https://github.com/bylaws/libadrenotools), [DXVK](https://github.com/doitsujin/dxvk), [Mesa Turnip](https://docs.mesa3d.org/drivers/freedreno.html)

## Disclaimer

T5Jool is a fan project, not affiliated with or endorsed by Activision, Treyarch or Microsoft. Call of Duty and Black Ops are trademarks of their respective owners. No game files are distributed.

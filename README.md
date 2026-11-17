<p align="center">
  <img src="docs/assets/logo.png" width="260" alt="T5Jool">
</p>

<h1 align="center">T5JOOL</h1>
<p align="center"><b>Black Ops Zombies Recomp</b><br>
A native 64-bit port of Black Ops Zombies for Linux and Android</p>

<p align="center">
  <a href="https://github.com/Joolace/T5Jool/releases"><img src="https://img.shields.io/badge/download-v0.1--alpha-e2242a?style=for-the-badge" alt="Download"></a>
  <a href="https://discord.gg/NXES3JBgax"><img src="https://img.shields.io/badge/discord-join-5865F2?style=for-the-badge" alt="Discord"></a>
  <a href="https://joolace.github.io/T5Jool/"><img src="https://img.shields.io/badge/website-visit-111114?style=for-the-badge" alt="Website"></a>
</p>

---

> [!CAUTION]
> **Alpha software, still in development.** Expect bugs, crashes and missing features. Save often, keep your expectations low, and please report what you find.

> [!IMPORTANT]
> T5Jool does **not** include any game files. You need your own legal copy of *Call of Duty: Black Ops* for PC.

## Features

- **Native, not emulated**: the game code runs as a 64-bit program on x86_64 Linux and arm64 Android
- **Vulkan renderer**: Direct3D 9 translated by DXVK, with a persistent shader cache
- **Upscalers**: Snapdragon GSR and FSR 1, render scale and sharpness options
- **LAN co-op**: host from the menu, automatic game discovery, join mid-round, PC and Android together
- **Controls**: controllers, handheld buttons, remapping, and a touch layout you can move and resize
- **New menus**: main menu, pause menu and loading screens with map previews, custom background and music
- **Every map**: all Black Ops Zombies maps you own, plus Dead Ops Arcade

## Supported platforms

| | Android | Linux |
|---|---|---|
| Architecture | arm64 | x86_64 |
| GPU | Adreno with a Turnip driver (required) | Any Vulkan GPU |
| Tested on | AYN Odin 3 Pro | Acer Nitro V16 (Ubuntu, NVIDIA GPU) |

Mali support is planned.

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
2. Download the latest APK from [Releases](https://github.com/Joolace/T5Jool/releases) and install it.
3. **Install a Turnip driver** (required on Adreno): the first time you press Play the launcher offers to download one. Pick a driver from Mr Purple or StevenMXZ and it is installed automatically. You can change it later in Settings > Download a driver, or import your own `.zip`.
4. In Settings choose the game folder.
5. Tap **Zombies** or **Dead Ops Arcade**.

> [!WARNING]
> The stock Qualcomm Vulkan driver does not work yet: the game closes at startup. Use Turnip.

The first launch compiles shaders, so the first minutes can stutter a little.

## LAN co-op

On the host: **Multiplayer (LAN) → Host LAN game** and pick a map.
On the other devices: **Multiplayer (LAN)** and select the game from the list. Everyone must be on the same network.
If you use a firewall, allow UDP ports **3074** (game) and **3075** (discovery).

## Reporting bugs

Use the **Logs** button in the launcher to save the logs, then share them on [Discord](https://discord.gg/NXES3JBgax) or in an issue.

## Roadmap

- [x] Native Linux and Android builds
- [x] Upscalers and resolution scaling
- [x] LAN co-op with discovery and join in progress
- [ ] Rumble, gyro aiming, mouse and keyboard on Android
- [ ] Mali GPU support
- [ ] Multiplayer: dedicated servers and online play
- [ ] Mods: custom maps, scripts and engine mods
- [ ] Source code release

## Source code

The source code is not published yet. It will be released later.

## Credits

- **Joolace**: development
- **MisaDev4**: [bo1-zombies-decompiled](https://github.com/MisaDev4/bo1-zombies-decompiled), the base of this port
- **iloupv**: contributions
- **[Mr Purple](https://github.com/MrPurple666/purple-turnip)** and **[StevenMXZ](https://github.com/StevenMXZ/Adreno-Tools-Drivers)**: the Turnip drivers T5Jool is tested with
- [SDL2](https://www.libsdl.org/), [DXVK](https://github.com/doitsujin/dxvk), [Mesa Turnip](https://docs.mesa3d.org/drivers/freedreno.html)

## Disclaimer

T5Jool is a fan project, not affiliated with or endorsed by Activision, Treyarch or Microsoft. Call of Duty and Black Ops are trademarks of their respective owners. No game files are distributed.

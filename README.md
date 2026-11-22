<p align="center">
  <img src="docs/assets/logo.png" width="260" alt="T5Jool">
</p>

<h1 align="center">T5JOOL</h1>
<p align="center"><b>Black Ops Zombies Recomp</b><br>
A native 64-bit port of Black Ops Zombies for Linux and Android</p>

<p align="center">
  <a href="https://github.com/Joolace/T5-Jool---Black-Ops-Recomp/releases"><img src="https://img.shields.io/badge/download-v0.6--alpha-e2242a?style=for-the-badge" alt="Download"></a>
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

## Features

- **Native, not emulated**: the game code runs as a 64-bit program on x86_64 Linux and arm64 Android
- **Vulkan renderer**: Direct3D 9 translated by DXVK, with a persistent shader cache
- **Upscalers**: Snapdragon GSR and FSR 1, render scale and sharpness options
- **Graphics presets**: Potato, Battery, Balanced, Quality, Smooth, including a Potato preset for weak PCs
- **Frame generation**: Lossless Scaling frame generation on Android and Linux, with your own copy of Lossless Scaling
- **FXAA** antialiasing on Android
- **Solo save**: a save at the start of every round, one per map, with Load game and Retry round after a game over
- **Online multiplayer**: dedicated servers with a server browser and ping, join mid-round, PC and Android together
- **LAN co-op**: host from the menu, automatic game discovery, join by address
- **Host your own server**: a Linux server with no graphics, several rooms and map rotation ([guide](SERVER.md))
- **Mods**: load community mods made with the Black Ops mod tools from the Mods menu
- **Controls**: controllers, handheld buttons, remapping, and a touch layout you can move and resize
- **Vibration and gyro aiming**: on the controller or the device itself, gyro always or only while aiming
- **Mouse and keyboard** on Android too
- **New menus**: main menu, pause menu and loading screens with map previews, custom background and music
- **Every map**: all Black Ops Zombies maps you own, plus Dead Ops Arcade
- **Automatic updates**: the Android launcher updates T5Jool and your Turnip driver for you

## Supported platforms

| | Android | Linux |
|---|---|---|
| Architecture | arm64 | x86_64 |
| GPU | Adreno: Turnip driver recommended, stock Qualcomm driver works | Any Vulkan GPU |
| Tested on | AYN Odin 3 Pro | Acer Nitro V16 (Ubuntu, NVIDIA GPU) |

Mali GPUs: **experimental, untested** (the launcher offers the PanVK driver). Reports welcome!

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
5. Tap **Play Zombies** or **Dead Ops Arcade**.

> [!NOTE]
> The stock Qualcomm Vulkan driver works from v0.5, but it is slower than Turnip and stutters while it compiles shaders
> (less and less as the shader cache fills). Turnip is still the best choice.

The first launch compiles shaders, so the first minutes can stutter a little.

## Multiplayer

**LAN co-op** - on the host: **Multiplayer → LAN → Host LAN game** and pick a map.
On the other devices: **Multiplayer → LAN** and select the game from the list. Everyone must be on the same network.
If you use a firewall, allow UDP ports **3074** (game) and **3075** (discovery).

**Online** - **Multiplayer → Online** lists the dedicated servers on the internet, with their ping.
A server that is not in the list: **Multiplayer → Join by address** and type its IP.
Want to run one? See **[Hosting a dedicated server](SERVER.md)** (Linux).

## Frame generation and antialiasing

**Android:** Settings → **Antialiasing (post-process) → FXAA** smooths jagged edges at a low cost.

**Android frame generation (experimental):** Settings → **Frame generation**. You need your own copy of
[Lossless Scaling](https://store.steampowered.com/app/993090/Lossless_Scaling/) on Steam: copy `Lossless.dll` from
`steamapps/common/Lossless Scaling` to your device and import it once (it stays in the app's private storage and is never
shared). Then choose 2x, 3x or 4x. Works best with a stable frame rate (set an FPS limit) and adds a little input lag.

**Linux:**
- **Frame generation** with [lsfg-vk](https://github.com/PancakeTAS/lsfg-vk) (Lossless Scaling, needs your own copy of
  Lossless Scaling on Steam). Install lsfg-vk, create a profile named `T5Jool` in `lsfg-vk-ui` (active for `BO1Zombies`),
  then start with `./t5jool.sh --lsfg`. Works best with a stable base frame rate (set an FPS limit) and adds a little input lag.
- **FXAA / SMAA** with [vkBasalt](https://github.com/DadSchoorse/vkBasalt): install it, choose the effects in
  `~/.config/vkBasalt/vkBasalt.conf`, then start with `./t5jool.sh --vkbasalt`. Don't use it together with lsfg-vk.

## Solo save

In solo, the game is saved at the start of every round from round 2, one save per map.
**Play Solo > Load game** shows your saves with round and date: continue or delete them. After a game over choose
**Retry round**, **New game** or **Return to menu**. Saved: round, points, weapons and ammo, perks, opened doors,
power. Not yet: mystery box spot, Pack-a-Punch links, equipment, Easter eggs. Turn it off in Options > Game.

## Mods

Put a community mod (made with the Black Ops mod tools) in the game folder as `mods/<name>/` with its `mod.ff`,
then choose it in **Mods** in the main menu: the game restarts with it. **None** goes back to the base game.
To play a mod online, everyone needs the same mod as the server.

## What's new

See the full [CHANGELOG](CHANGELOG.md). Highlights of **v0.6-alpha**:
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
- [ ] Mali GPU support (experimental, needs testers)
- [x] Dedicated servers (Linux) and online server browser (v0.3)
- [x] Mods menu for community mods (v0.3)
- [x] Lossless Scaling frame generation and FXAA (v0.4)
- [x] Save and resume in solo (v0.5)
- [x] Stock Qualcomm driver on Adreno (v0.5)
- [x] Faster save loading and Potato graphics preset (v0.6)
- [ ] Seamless map change on servers (no reconnect)
- [ ] Custom maps and engine mods
- [ ] Fuller solo saves (mystery box, Pack-a-Punch links, equipment)
- [ ] Source code release

## Source code

The source code is not published yet. It will be released later.

## Credits

- **Joolace**: development
- **MisaDev4**: [bo1-zombies-decompiled](https://github.com/MisaDev4/bo1-zombies-decompiled), the base of this port
- **iloupv**: contributions
- **[micycle1](https://github.com/micycle1/bo1-zombies-decompiled)**: engine fixes (lighting, net fields, decompile bugs)
- **[Mr Purple](https://github.com/MrPurple666/purple-turnip)** and **[StevenMXZ](https://github.com/StevenMXZ/Adreno-Tools-Drivers)**: the Turnip drivers T5Jool is tested with
- [lsfg-vk](https://github.com/PancakeTAS/lsfg-vk) by PancakeTAS and its Android branch by [GameNative](https://github.com/GameNative/lsfg-vk-android) (MIT): frame generation. Lossless Scaling itself is a separate paid app and is not included
- [SDL2](https://www.libsdl.org/), [DXVK](https://github.com/doitsujin/dxvk), [Mesa Turnip](https://docs.mesa3d.org/drivers/freedreno.html)

## Disclaimer

T5Jool is a fan project, not affiliated with or endorsed by Activision, Treyarch or Microsoft. Call of Duty and Black Ops are trademarks of their respective owners. No game files are distributed.

# Changelog

## v0.5-alpha
*November 2026*

### Highlights
- **Stock Qualcomm driver works**: Turnip is no longer required on Adreno (it is still faster and stutters less)
- **Solo save** (experimental): your solo game is saved at the start of every round from round 2, one save per map. **Play Solo > Load game** lists them with round and date; New game warns you when the map already has a save

### Solo save
- Main menu: Play is now **Play Solo**, with New game and Load game
- Saved: round, points, weapons with their ammo, perks, opened doors / debris / airlocks, power, and the next special round (dogs, thief, monkeys)
- Not saved yet: the mystery box spot, Pack-a-Punch / teleporter links, equipment (Moon's gas mask and hacker), knife upgrades, claymores, map Easter eggs
- **Game over menu** in solo when the map has a save: **Retry round N** (from the beginning of the round you died in), **New game** or **Return to menu**. The save stays, so Load game can also retry that round later
- Load game > a save > **Delete save** (press twice) removes it. Turn saving off in Options > Game > Solo save
- Files: `t5jool/saves/<map>.sav` in the T5Jool home folder. Script part: `mods/t5save` (loaded automatically, does nothing in co-op or on servers)

### Android
- **New launcher home**: big Play Zombies and Dead Ops Arcade buttons over the menu background, Settings / Report a bug / Discord buttons, and status cards on the right (game files, last solo save, graphics summary, driver) that open the right page when tapped
- **New launcher settings**: categories on the left (Game, Graphics, Frame generation, Controls, Touch and gyro, Advanced, Help and about), a short explanation under every option, short choices as buttons instead of pop-up lists
- **Help and about** page: T5Jool card with logo and version, your device with icons (processor, graphics, memory, system, display, storage), Discord banner in its own colours
- **Graphics presets**: Battery, Balanced, Quality and Smooth set resolution, quality, FPS limit and antialiasing in one tap
- **Loading screen** while the game restarts for a new map or a loaded save (it restarts to free memory) and while it starts: the map name and what is happening, instead of a black screen and a flash of the launcher

### Fixes
- Android: Turnip's shader cache is now really kept between games (it was always empty: every start compiled all shaders again). Faster map loading, less stutter
- Android: DXVK no longer fails to allocate memory when the preferred memory type is not allowed for a resource (the stock Qualcomm driver allows only some types for buffers): the game now runs on the stock driver
- Android: mods with scripts (horde, zinfo, the solo save) are in the app again: folders starting with "_" (mods/_stack) were left out of the APK
- No more crash after an error in the main menu (for example "Bad server address" when joining): the game closed with "Exceeded limit of 16 'font' assets"
- D-pad HUD: the equipment highlight, the ammo circle and the ammo counters now use their real images. Fixes the checkerboard square on Moon (missing HUD images are also no longer drawn)

## v0.4-alpha
*November 2026*

### Highlights
- **Lossless Scaling frame generation on Android** (experimental): up to 2x, 3x or 4x the frames on screen, with your own copy of Lossless Scaling
- **FXAA antialiasing** on Android, much lighter than MSAA
- **Fixes for online play**: a crash on Moon, automatic updates from the right GitHub page

### Graphics
- **Frame generation** (Android, experimental): Settings > Frame generation. Import your own `Lossless.dll` once (from your Steam copy of Lossless Scaling: it stays in the app's private storage), then choose 2x / 3x / 4x, flow scale and performance mode. Built on lsfg-vk (MIT) by PancakeTAS and the Android branch by GameNative
- With frame generation on, Show FPS shows both numbers, e.g. `60 FPS -> 120 LSFG`
- **FXAA** (Android): Settings > Antialiasing (post-process). Smooths jagged edges for about 1 ms per frame
- Linux: `./t5jool.sh --lsfg` for Lossless Scaling frame generation with lsfg-vk, `./t5jool.sh --vkbasalt` for FXAA/SMAA with vkBasalt

### Fixes
- Mods: `mod.ff` and custom map files are found in the game folder too (the engine only looked in the T5Jool home folder, so mods chosen in the Mods menu did not load)
- Online: no more crash when a weapon is used before your game has received it (Moon: gas mask and then a grenade)
- Moon: no more "sm_sunSampleSizeNear is cheat protected" flood (hundreds of lines a second in the log, extra work every frame)
- Android launcher: automatic updates and bug reports now use the right GitHub repository (T5-Jool---Black-Ops-Recomp). If the launcher does not offer this update, download it by hand once from the Releases page

## v0.3-alpha
*November 2026*

### Highlights
- **Online multiplayer with dedicated servers**: Multiplayer > Online lists the servers on the internet with their ping, and you join mid-round
- **Host your own server** on Linux, without graphics: several rooms on one machine, map rotation, automatic restart. Guide: [SERVER.md](SERVER.md)
- **Join by address** for servers that are not in the list
- **Mods**: a new Mods page loads community mods made with the Black Ops mod tools
- **Ping** on screen and in the scoreboard

### Multiplayer
- Multiplayer menu split into **LAN** (host or join on the same Wi-Fi), **Online** (dedicated servers) and **Join by address** (type an IP; the last one is remembered)
- Joining a server on the internet works (it used to wait for an online login that T5Jool does not have)
- **Ping indicator** at the top right when you play on someone else's game (green / yellow / red); can be turned off in Options > Show ping or in the Android launcher
- **Scoreboard** in co-op and on servers: points, kills, headshots and the ping of every player (no more empty columns)
- Zombies games always show at most 4 players

### Dedicated servers (Linux)
- `./t5jool-server.sh` runs a server with no graphics or sound, for a PC or a VPS. Settings in `~/.config/T5Jool/server.cfg`: name, map, max players, port, mod
- **Several rooms on one machine**: `./t5jool-server.sh <room>` runs another server with its own map and port
- **Map rotation**: `ROTATION="map1 map2 ..."`, one match per map; players reconnect to the new map by themselves
- At the end of a match the server starts a new one on the same map, with the players still connected
- An empty server restarts its match from round 1 after a minute
- Automatic restart after a crash; Ctrl+C stops it; the script checks missing system libraries and game files
- Master server for the online list: `tools/master/t5jool_master.py` (Python only, no dependencies)
- No more "Field faction changed ... MG42" warnings

### Mods
- **Mods** page in the main menu: lists the mods in the game folder (`mods/<name>/` with `mod.ff`); choosing one restarts the game with it, "None" goes back to the base game
- Mods get larger asset pools (more weapons, models, scripts and tables)
- Dedicated servers can run a mod (`MOD="name"`); players need the same mod

### Launcher (Android)
- **DXVK overlay** can be customized: choose what it shows (FPS, frame times, GPU load, GPU memory, GPU and driver, draw calls...) and its size
- New option: Show ping (online games)
- The bug report zip keeps only the recent logs (it could contain more than 100 files)
- Build: uses Java 21 or 17 by itself when the system Java is too new for Gradle

### Menus and fixes
- The **Test** page (developer helpers and power-ups) is no longer in the main and pause menus
- After touching the screen in a menu, the controller can move through the menu again
- Credits start from the top every time you open them, and you can scroll them (up/down, mouse wheel, or drag with your finger)
- Logs: a performance line every 30 s of play (average FPS, 1% low, worst frame), to track down FPS drops

## v0.2-alpha
*November 2026*

### Highlights
- **Much smoother**: no more GPU stalls from light glows, lighter logging, CPU build tuned for modern Snapdragon chips
- **Vibration, gyro aiming, mouse and keyboard** on Android
- **Automatic updates** for T5Jool and your Turnip driver
- **Experimental Mali GPU support** with the PanVK driver
- Kino's flickering lamps and Der Riese's green spawn yard fixed

### Performance
- Light glows (lamp halos) no longer make the CPU wait for the GPU every frame: the GPU is no longer stuck at 100% and frame pacing is steadier
- Console logging is buffered instead of written line by line: fewer stutters when many messages arrive at once (end of round)
- Android build optimized for ARMv8.2-A (see Build)

### Graphics
- Kino der Toten: lamps and the lobby sign no longer flicker (client-script lights keep their state between snapshots)
- Der Riese: the spawn yard is no longer washed green by the transformer sparks (FX light radius clamp)
- Correct bloom tint and spotlight direction

### Engine
- Co-op: player and corpse loop sounds sync correctly (no more "Field ... changed" errors)
- Holding the scoreboard no longer stops your movement
- Fixes to pathfinding, aim targets, sound occlusion, scorch marks, moving-object effects, third-person camera and rotation math
- Thanks to **micycle1** for the lighting, net field and decompile fixes

### Options
- The crosshair is always on at startup (an old menu setting could leave it hidden)
- **Show FPS** works again, with a bigger counter
- **Audio**: master, music, sound effects, voices, cinematics and menu music volumes
- New **Depth prepass** option (try it off on handhelds)
- Small T5Jool version label in the bottom-right corner, always visible (useful in screenshots and bug reports)

### Controls
- **Vibration**: firing (stronger for shotguns and launchers), taking damage and explosions. On the controller, or on the device's own motor on Android when the controller can't vibrate. Strength adjustable
- **Gyro aiming**: from the controller (DualSense, Switch Pro...) or the device gyroscope on Android. Always or only while aiming down sights, with sensitivity and axis inversion. Works in any screen orientation
- Vibration and gyro settings are in Options > Controls in game, and in the launcher Settings > Controls on Android
- **Mouse and keyboard on Android**: using them hides the touch controls, like a controller

### GPUs
- **Experimental support for non-Adreno GPUs** (Mali and others, untested): custom Vulkan drivers that are not for Qualcomm (like Mesa PanVK) are loaded directly instead of through libadrenotools
- The launcher offers the **PanVK** driver by FristOneRR on Mali GPUs (Settings > Download a driver) and shows a one-time "Experimental GPU" notice
- The game log now reports the device GPU and how the Vulkan driver was loaded

### Logs
- New **Game logs** setting in the launcher: errors only, normal (default) or detailed
- Console output is buffered and written once a second instead of line by line, and no longer copied to logcat: less work every frame
- Repeated lines and known noisy messages are collapsed, so logs are smaller and easier to read
- Stutters in game ("hitch") are written to the log with their duration and server time, to find their cause
- Linux: `T5_LOG=0/1/2 ./t5jool.sh` sets the same levels

### Launcher (Android)
- **Automatic updates**: checks GitHub for new T5Jool versions and installs them
- **Driver updates**: tells you when a newer version of your Turnip driver is out
- Driver download lists both Mr Purple and StevenMXZ drivers

### Build
- Single version number in `src/t5jool_version.h`, used by the game, the APK and the Linux package
- Two Android builds: **arm64-v8.2** (optimized for Snapdragon 845 and newer: faster atomics and math on the CPU side) and **arm64** (generic, for every 64-bit device)
- The launcher warns if the v8.2 build is installed on an older CPU, and the updater downloads the same variant you have

## v0.1-alpha
*November 2026*

First public alpha.
- Native 64-bit builds for Android (arm64) and Linux (x86_64), Vulkan rendering through DXVK with shader cache
- Snapdragon GSR and FSR 1 upscalers, render scale and sharpness
- LAN co-op from the Multiplayer menu: automatic game discovery, join in progress, PC and Android together
- Controllers, button remapping, movable and resizable touch controls
- New main menu, pause menu and loading screens with map previews and original menu music
- All Zombies maps plus Dead Ops Arcade
- Android launcher: splash screen, resolution / upscaler / FPS limit / V-Sync settings, automatic Turnip driver download, bug and crash reporting to GitHub or Discord
- Each new map starts in a fresh process on Android to avoid running out of memory
- Linux package with `t5jool.sh`: asks for the game folder and player name, `--install` adds a menu entry

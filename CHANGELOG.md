# Changelog

## v0.10-alpha
*November 2026*

> A new engine: T5Jool now runs on [OpenBLOPS-NX](https://github.com/HDErick/OpenBLOPS-NX) by HDErick, NaGaa95 and the OpenBLOPS team, with the T5Jool Linux and Android layer on top. The source code is released under GPL-3.0.

### Highlights
- **New engine** (OpenBLOPS-NX, native 64-bit): **Campaign**, Zombies and Multiplayer in one port
- **Source code released** under GPL-3.0, with build scripts for Linux and Android. The v0.8 code is in the `legacy-0.8` branch
- **Multiplayer**: Offline Match against bots, **Host Match** and the **community server browser** (Unranked tab), direct connect, and a relay so no port forwarding is needed
- **Touch-only play**: menus work with on-screen pad buttons and a D-pad, smoother touch aim in game
- **Mods are back**, and every launcher setting reaches the new engine

### First start
- The game's zones are converted once to the new 64-bit format (`kbz/` folder). On Android the launcher does it at the first start, with a progress bar. It takes a few minutes and needs some free space
- On Linux: `tools/t5/convert_zones.sh <game folder>`

### Controls
- **Menus without a controller**: on-screen D-pad (bottom left), A, B, X, Y (bottom right), LB, RB, LT, RT, Start and Back. They work in every menu, the pause menu and the team and class menus too
- A tap on a menu item still selects and presses it
- A pad button never also clicks the menu item behind it
- **Touch aim** and **mouse look** work in Multiplayer
- **Aim smoothing** for touch aim: new slider in the launcher (Touch and gyro)
- **Back and the B button no longer close the game** on Android; leave with Quit in the game or Home
- **Gyro aiming** uses the device's own gyro when the controller has none (handhelds, phones). Modes: Always or While aiming, with sensitivity and invert
- **Vibration**: on/off and strength from the launcher; with the touch controls the device itself vibrates

### Mods
- Choose a mod in the launcher (**Settings > Game > Mods**). Put each mod in `mods/<name>/` in the game folder, with its `mod.ff`
- The mod's zones are converted at the first start, like the game's
- Linux: `convert_zones.sh` converts mods too; start with `+set fs_game mods/<name>`

### Settings
- **Field of view** up to 110 degrees: launcher slider, and the game's own FOV setting now works
- **FPS limit** from the launcher works (the game was capped at 60)
- **Resolution scale and upscaler** work again: the game renders smaller and DXVK upscales, and touches land where you tap
- **FPS counter** uses the game's own counter

### Launcher
- New **Mods** section, **Field of view** and **Aim smoothing** sliders
- Dead Ops Arcade, the solo save card and the ping toggle are removed for now (not on the new engine yet)
- Zone conversion with a progress bar at the first start

### Fixes
- **Summit (mp_mountain) froze when the match started**: breaking glass looped forever
- **Broken glass now falls**: every shard used to come out with zero area and stayed in the air (all maps)
- No more crash when quitting on Android
- Local split-screen is off on Android, so the handheld's pad is never taken by a second player

### Not on the new engine yet
- Solo saves, Dead Ops Arcade, Zombies co-op online and the T5Jool dedicated servers ([SERVER.md](SERVER.md) is for v0.8)

## v0.8-alpha
*November 2026*

> T5Jool is no longer Zombies only: a new menu with Zombies and Multiplayer, games and matches anyone can join from the online list, and the relay for players behind a router or a mobile network.

### Highlights
- **New menu and launcher** with Zombies and Multiplayer sections
- **Host online games** (Zombies co-op) and **online matches** (Multiplayer, beta) that anyone can find, with UPnP or the master server relay
- Includes every fix of v0.7 and v0.7.1 (low memory devices, Mali with PanVK, touch, gamepads, wide screens)

### Menus
- **New menu layout**: the main menu has **Zombies** (Solo, Co-op online, Co-op on local network) and **Multiplayer** (Play vs bots, Host online match, Find matches), then Join by address, Options, Mods, Credits and Quit. Each online list shows only its own kind of game
- **Launcher**: T5Jool is now "Black Ops Recomp". The home screen has two large cards side by side, **ZOMBIES** and **MULTIPLAYER** (opens the game on the Multiplayer page, with its own picture), with Dead Ops Arcade, Settings, Report a bug and Discord in one row below. More space between items, and graphics and driver are shown in a single card. The card pictures can be replaced with your own: put `card_mp.jpg` (or `card_zombies.jpg`, also .png or .webp) in the T5Jool folder. Small buttons no longer wrap their text
- After a multiplayer match, and after Quit to menu from one, the game comes back to the Multiplayer page

### Online
- **Host online game** (Zombies > Co-op online): host a Zombies co-op game that anyone can find in the online list. The game opens the router ports by itself with UPnP (home Wi-Fi), and the pause menu shows whether other players can reach you
- The online list only shows games that can really be joined: the master server checks each one from another port. Games hosted behind a router without UPnP, or on a mobile network, are not listed (friends can still use Join by address)
- **Relay for hosted games**: when a hosted game is not reachable (router without UPnP, mobile network, CGNAT), it goes through the master server's relay. Players join it from the online list like any server, also with older versions of T5Jool
- **Online matches (beta)**: Multiplayer > Host online match hosts a multiplayer match that anyone can find in Multiplayer > Find matches (players only: the game scripts add no bots to network matches), with UPnP or the relay like Zombies
- Online list: a game hosted through the relay was shown with the wrong port (the host's own 3074), so joining it connected to the dedicated server on the same machine. The game port now comes from the port that answered, and pings are matched by address and port
- Joining a game from the list switches the game to multiplayer or Zombies by itself when needed (a multiplayer match needs a restart in multiplayer mode)
- Master server (`tools/master`): reachability check from UDP port 3077, relay on UDP 3078 and 3100-3199, which must be open on the VPS

## v0.7.1-experimental
*November 2026*

> Experimental update of v0.7: Mali GPUs with PanVK reach the game, more touch controls, touch scrolling in the menus.

### Highlights
- **Mali GPUs with PanVK: fixed the crash right after startup** (tested in game on a Mali-G615)
- **Touch controls in multiplayer**, a **second fire button** and a **D-pad**
- **Menu lists scroll with a finger**, and Versus has Start match at the top

### Update (same version)
- **Gamepads**: a `gamecontrollerdb.txt` (the community mapping database for SDL) in the game folder or in `t5jool/` adds mappings for pads SDL does not know. The log lists every pad with its name and type (Xbox, PS4, PS5, Switch Pro...), and says when one is ignored
- Touch: **fixed the menu scrolling by itself** after a tap, and taps on an item work at the first try (the jump of the cursor to the finger no longer counts as a drag, and small finger movements are ignored). The finger lift did not always reach the menu, so the list kept following the cursor; the menu now reads whether the finger is still down
- Fewer log lines during play: the memory diagnostics printed a line every time an image streamed in, which could cause small stutters

### Fixes
- **No more stretched image on screens wider than 16:9** (most phones, 21:9 monitors): with the aspect ratio on auto the game stopped at 16:9 and stretched the picture sideways. It now uses the real screen ratio, with a wider field of view
- **Gamepads in multiplayer (Versus)**: the pad worked in Zombies but not in multiplayer. Multiplayer starts straight into the map, so the pad button and stick bindings, loaded at the first main menu, were never loaded. This also affected the touch controls, which are a virtual pad
- Mali GPUs: when the graphics do not start, the error window explains that the stock Mali driver is not enough and a PanVK driver is needed (or, with a custom driver, to try another PanVK version)
- **Touch: the menu lists scroll by dragging a finger** (Options, Versus, maps, Create a Class...). A tap still selects: the item is chosen when the finger lifts, and only if it did not move
- Versus: **Start match is now at the top** of the page. On short screens, and with touch, the list did not scroll down to it
- **Mali GPUs with PanVK: fixed a crash right after startup** (Mali-G615): the driver reports "present wait", but on Android the swapchain belongs to the system Vulkan loader and the call jumped to an invalid address. DXVK no longer uses present wait on Android
- A Turnip driver (for Adreno) chosen on a phone without an Adreno GPU is now skipped with a clear message in the log, instead of failing graphics startup

### Touch controls
- **Touch controls in multiplayer** (Versus): they never appeared, because in a multiplayer match other input states stay on besides menu and console
- **Second fire button** on the left, above the stick, to shoot with either hand
- **D-pad** (up, down, left, right) in the top left: on Moon the P.E.S. helmet and the special items use it
- All new buttons can be moved and resized in Touch controls layout; layouts saved before keep working

## v0.7-experimental
*November 2026*

> **Experimental build for testing more devices.** It fixes "Out of memory" on phones with less low memory and adds Versus (beta), but several changes have had little testing. If something breaks, please send your logs (launcher > Logs) on Discord with your device model.

### Highlights
- **Less "Out of memory" on Kino der Toten, Five and Der Riese** on phones with less low memory (tested on a Motorola edge 70 max)
- **Versus (beta)**: an offline multiplayer match against bots from the main menu, with native team and class menus and **Create a Class**
- **Mali GPUs (experimental)**: custom PanVK drivers are now loaded the same way as Turnip on Adreno
- Several 64-bit crash fixes: explosions near cars and barrels, player bodies, vehicles, map unload

### Fixes
- Custom Vulkan drivers on Mali GPUs (PanVK) load through the driver namespace like Turnip on Adreno. They failed with "libhardware.so not found", and the stock Mali driver lacks VK_EXT_robustness2, which DXVK needs
- Android: Quit to menu (pause) and Return to menu (game over) restart the game into the menu with clean memory. On phones with less low memory the disconnect itself ran out of memory
- **Less "Out of memory" when loading Kino der Toten, Five and Der Riese on phones with less low memory** (seen on Snapdragon 778G and SM8845): a map needs about 190 MB in one block, and only the largest free zone below 2 GB can hold it. Other large blocks now go into that zone only when they fit nowhere else, and the skinned vertex cache, index buffers and temporary image memory (about 40 MB) no longer use memory below 2 GB
- **Fixed a crash when a multiplayer map unloads** (end of match, map rotation): invalid material pointers in the multiplayer menus are skipped while the game checks which assets are still in use
- **Fixed a crash when a vehicle spawns** (the RC-XD killstreak in the multiplayer test boot): the vehicle's collision helper lost its function table when the vehicle was cleared
- **Fixed a crash when the player dies and leaves a body** (blown up next to a car in the multiplayer test boot): six places looked up the player with the 32-bit position of the entity's class name. This also affected zombie corpses and AI pathing
- **Fixed a crash when something explodes near destructible objects** (barrels, cars, crates): the queued area damage lost the upper half of the object's address on 64-bit. Seen on Verrückt and in the multiplayer test boot, after a few minutes of play

### Development
- Low memory diagnostics for the "Out of memory" on some phones (Kino der Toten, Five, Der Riese): the log lists the large allocations below 2 GB and, when memory runs out, the address space below 4 GB. Two test switches: `+set bo1_lowmem_waste N` takes N MB of low memory to reproduce a phone with less of it, and `+set bo1_pmem_ext 1` loads large fastfiles between 2 and 4 GB (not safe yet: it crashes in places that keep pointers in 32-bit integers)
- Model lighting no longer passes a pointer through a 32-bit integer (first of those places)
- **Versus (beta)** in the main menu: an offline multiplayer match against bots without typing a command line. Choose the mode, map, time limit, friendly and enemy bots (Team Deathmatch and Free-for-all) and their difficulty, then Start match. The game restarts in multiplayer with every item unlocked. The choices are saved
- **Create a Class (Versus)**: five custom classes with every weapon, attachment, perk, grenade, equipment and killstreak. In a match the team and class choices use the native menu (touch, pad and keyboard), and "Edit custom classes" opens the editor. Classes are saved in `t5jool/classes.txt` in the game's home folder. The offline game read custom classes from a function that always returned nothing on PC
- Create a Class: choosing a custom class again reloads it, so changes made in the editor apply from the next spawn. Killstreaks can be chosen
- Fixed a crash in the graphics driver when a Versus match ends and the game restarts into the menu
- The native team and class menus work even when the native menu is turned off in the multiplayer settings
- Versus: the choices no longer jump back to the first option. At the end of a match the game restarts into the T5Jool menu instead of closing (it looked like a crash)
- Multiplayer test boot (`+set bo1_zombies 0`): the Zombies solo save mod is not loaded
- Multiplayer test boot: fixed a freeze after startup and while loading the map (the loading screen waited for a file the game was still loading)
- Multiplayer test boot: fixed a crash right after the map loaded (the Zombies screen effects are missing on multiplayer maps and are now skipped)
- Multiplayer test boot: if no frame starts for 60 seconds the game stops and writes where every thread got stuck
- Android crash log: also lists the registers and stack entries that point into the game code, so a crash with a broken call stack can still be traced
- Android: stack protection in the game code (a stack overwrite stops in the function that caused it) and the build ID in the log, so crash reports are always translated with the right symbols
- Android build: the symbols saved for crash reports now come from the library just built (an old one from a previous build configuration was picked)
- **Fixed a stack overwrite when a map loads** (exposure settings wrote past the map name buffer): it crashed the multiplayer test boot on Android right after the map loaded
- Multiplayer test boot: **multiplayer stats work**: the stats buffers are 40 KB (the launch-day exe had 40168 bytes, the updated game files need 40548), in the player's storage, the server's per-client copies and the stats sent between them. Before, the first stat written (blowing up a car) ended the match, then stats were skipped
- Multiplayer test boot: log lines for the team and class menus (`T5Jool menu:`), including the keys they receive
- Multiplayer test boot: the gamepad D-pad and left stick move through the original menus (team, class), A selects; a tap selects the item under the finger
- Menus: framed panels whose material is missing from the game files are no longer drawn as a checkerboard (multiplayer Settings panel)
- Android: the touch screen now moves the menu cursor of the original game menus (the cursor position stayed at the corner)
- Multiplayer test boot: bots (Combat Training) move, shoot and knife: the script only enabled them in developer builds
- Multiplayer test boot (Android): leaving a match goes back to the launcher instead of crashing while the multiplayer menus are reloaded
- Multiplayer test boot: the pause button opens the original multiplayer menu (choose team and class) instead of the Zombies pause menu
- Android "Out of memory": the log now shows how much low memory is free and how it is split, to tell "used up" from "fragmented"
- Startup watchdog: if the game is still not past startup after 45 seconds it stops and writes where every game thread got stuck to the log (`BO1_INIT_TIMEOUT`, 0 = off)

## v0.6-alpha
*November 2026*

### Highlights
- **Faster save loading**: perks, weapons, power and doors come back almost at once
- **Potato graphics preset** for weak PCs and devices, in game and in the Android launcher
- Android: the launcher's graphics quality now really changes texture detail

### Solo save
- **Much faster loading of a save**: perks come back at once (no bottle animation at each machine), power and doors are restored at the same time as weapons and perks

### Graphics
- **Potato preset** for weak PCs and devices: in game, Options > Graphics > Preset: Potato (lowest textures, no shadows, depth of field, glow, distortion, impact marks or clouds, simpler models; on PC also 1280x720). Preset: High brings the original look back
- Android launcher: new **Potato** graphics preset (540p, 30 FPS, Potato quality) and a Potato level in Graphics quality
- Android: the graphics quality chosen in the launcher now really changes the texture detail (the game was choosing it by itself and ignored it)

### For modders
- `+set t5_dumpscripts 1` saves the game scripts loaded from your own fastfiles to `t5jool/scriptdump` (off by default), to write mods and features against the real scripts

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

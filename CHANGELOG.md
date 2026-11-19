# Changelog

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

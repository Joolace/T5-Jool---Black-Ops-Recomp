# T5Jool per Android (alpha)

Port nativo arm64-v8a: lo stesso motore della build Linux a 64 bit, con SDL2 per finestra/input/audio e
DXVK (Direct3D 9 → Vulkan). Pensato per console e telefoni Snapdragon recenti (es. AYN Odin 3, 8 Elite).

Community e segnalazioni: [Discord di T5Jool](https://discord.gg/rcMeKZcShx)

## Compilare (Ubuntu)

```
sudo apt install git curl unzip openjdk-17-jdk-headless meson ninja-build pkg-config python3 make glslang-tools
tools/android/setup.sh      # SDK, NDK, SDL2, DXVK, libadrenotools, FFmpeg in ~/t5jool-android (una volta)
tools/android/build.sh      # -> out/T5Jool-alpha.apk   (build.sh install = anche adb install)
```

## Sul dispositivo

1. Installa `T5Jool-alpha.apk`.
2. Copia la cartella di Black Ops (PC) con `main/` e `zone/` in `/sdcard/T5Jool/game/`
   (la cartella si cambia nel lanciatore).
3. Apri T5Jool, tocca **Storage access** e consenti l'accesso a tutti i file, poi **PLAY**.

## Lanciatore

- **Vulkan driver**: driver di sistema (Qualcomm) o un driver importato da uno zip nel formato di
  adrenotools (con `meta.json`, es. Turnip). Se il driver personalizzato non si carica il gioco usa
  quello di sistema e lo scrive nel log.
- **Resolution**: scala della superficie (il compositore di Android la ingrandisce a tutto schermo).
- **Quality / FPS limit / Show FPS / DXVK overlay**.
- **Touch controls**: opacità, dimensione, velocità della mira. Con un controller in uso spariscono.
- **Extra command line**: argomenti in più per il motore (`+set ...`).

## Log e crash

Tutto in `/sdcard/T5Jool/logs/`:

- `game.txt` — console del gioco, DXVK, script (`game.prev.txt` è la sessione precedente);
- `crash-<data>.txt` — segnale, registri e stack di un crash;
- `logcat-crash-<data>.txt` — logcat salvato dal lanciatore alla riapertura dopo un crash.

**Export logs to Download** crea `Download/T5Jool-logs-<data>.zip` con tutto (ultimi 3 giorni) e le
impostazioni usate. Sul PC, `tools/android/symbolize.sh crash-<data>.txt` traduce lo stack in funzioni
e righe usando `out/libmain-symbols.so` della stessa build.

## Come funziona (note tecniche)

- Il motore a 64 bit tiene indirizzi in interi a 32 bit: tutta la sua memoria sta sotto i 2 GB
  (`src/platform/bo1_lowmem.cpp`). Su Android `libmain.so` è PIE, quindi `t5launcher.c` riserva una zona
  libera sotto i 2 GB e ci carica la libreria con `android_dlopen_ext(ANDROID_DLEXT_RESERVED_ADDRESS)`
  prima che SDL la cerchi.
- `malloc`/`new` del motore restano interni a `libmain.so` (`-Bsymbolic` + `android/libmain.map`): SDL,
  DXVK e il driver Vulkan usano l'allocatore normale di Android.
- Il gioco gira nel processo `:game`; il lanciatore resta vivo e, se trova il file "partita in corso"
  ancora presente, salva logcat.
- Driver personalizzati: `libadrenotools` carica il driver e passa il suo `vkGetInstanceProcAddr` a DXVK
  (`DXVK_ANDROID_GIPA`, patch `android/patches/dxvk-android.patch`, che crea anche la superficie Vulkan
  con quel caricatore).

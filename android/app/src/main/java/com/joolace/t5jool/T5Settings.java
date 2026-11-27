package com.joolace.t5jool;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Environment;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Impostazioni del lanciatore, lette anche dal processo del gioco quando parte. */
final class T5Settings {
    static final String PREFS = "t5jool";

    static final String[] RES_LABELS = { "100% (native)", "90%", "80%", "75%", "67%", "58%", "50%" };
    static final int[] RES_VALUES = { 100, 90, 80, 75, 67, 58, 50 };
    static final String[] UPSCALER_LABELS = { "Snapdragon GSR 1 (recommended on Adreno)", "AMD FSR 1", "Bilinear" };
    static final String[] UPSCALER_VALUES = { "sgsr", "fsr", "bilinear" };
    static final String[] SHARP_LABELS = { "Soft (1.0)", "1.25", "1.5", "1.75", "Sharp (2.0)" };
    static final int[] SHARP_VALUES = { 100, 125, 150, 175, 200 };
    static final String[] QUALITY_LABELS = { "Low", "Medium", "High", "Potato" }; // 3 in fondo: i valori salvati restano validi
    static final String[] FPS_LABELS = { "30", "45", "60", "90", "120", "Unlimited" };
    static final int[] FPS_VALUES = { 30, 45, 60, 90, 120, 0 };

    String gameDir;
    String driver;      // "" = driver di sistema, altrimenti nome della cartella in files/drivers
    int resolution;     // percentuale (vecchia impostazione, usata se renderHeight e' 0)
    int renderHeight;   // altezza di rendering in pixel (0 = nativa / usa resolution)
    int quality;        // 0 basso, 1 medio, 2 alto
    int upscaler;       // indice in UPSCALER_VALUES (usato con risoluzione < 100%)
    int postAa;         // antialiasing dopo l'immagine: 0 spento, 1 FXAA (T5_POSTAA, patch DXVK)
    // frame generation di Lossless Scaling (lsfg-vk, liblsfg-vk.so caricata da DXVK con T5_LSFG)
    boolean lsfg;
    int lsfgMultiplier;   // indice in LSFG_MULT_VALUES
    int lsfgFlow = 1;     // indice in LSFG_FLOW_VALUES
    boolean lsfgPerformance = true;
    static final String[] LSFG_MULT_LABELS = { "2x", "3x", "4x" };
    static final int[] LSFG_MULT_VALUES = { 2, 3, 4 };
    static final String[] LSFG_FLOW_LABELS = { "100%", "75%", "50%" };
    static final String[] LSFG_FLOW_VALUES = { "1.0", "0.75", "0.5" };

    static File lsfgDll(Context ctx) {
        return new File(ctx.getFilesDir(), "lsfg/Lossless.dll");
    }
    static final String[] POSTAA_LABELS = { "Off", "FXAA (light, replaces the upscaler)" };
    int sharpness;      // 100..200 (SGSR)
    int fps;            // 0 = senza limite
    boolean showFps;
    boolean showPing = true;
    boolean vsync; // spento: meno input lag, il limite FPS resta attivo
    boolean rumble = true;     // vibrazione (controller o dispositivo)
    int rumbleStrength = 70;   // %
    int gyro = 0;              // 0 spento, 1 sempre, 2 solo mirando
    int gyroSens = 150;        // %
    boolean gyroInvertX, gyroInvertY;
    static final String[] GYRO_LABELS = { "Off", "Always", "While aiming" };
    int logLevel = 1; // 0 = solo errori, 1 = normale, 2 = dettagliato (T5_LOG)
    static final String[] LOG_LABELS = { "Errors only (fastest)", "Normal (recommended)", "Detailed (for debugging)" };
    boolean dxvkHud;
    /** voci dell'overlay di DXVK (DXVK_HUD), separate da virgole */
    String dxvkHudItems = "fps,frametimes,gpuload,version";
    int dxvkHudScale = 1; // indice di HUD_SCALES
    static final String[] HUD_KEYS = { "fps", "frametimes", "gpuload", "memory", "devinfo", "version", "api",
        "drawcalls", "submissions", "pipelines", "compiler" };
    static final String[] HUD_LABELS = { "FPS", "Frame times (graph)", "GPU load", "GPU memory", "GPU and driver",
        "DXVK version", "Vulkan API version", "Draw calls", "Submissions", "Pipelines", "Shader compiling" };
    static final String[] HUD_SCALES = { "0.75", "1.0", "1.25", "1.5" };
    static final String[] HUD_SCALE_LABELS = { "Small", "Normal", "Large", "Extra large" };
    boolean touch;
    int touchOpacity;   // 10..100
    int touchScale;     // 60..160
    int touchLook;      // 20..400 (100 = 1.0)
    int touchSmooth = 50; // 0..100 (bo1_touch_smooth = /100)
    int fov = 65;       // 65 = impostazione del gioco, fino a 110
    String extraArgs;
    String playerName;
    String mod = "";    // cartella in <gioco>/mods, "" = gioco base
    String envVars;     // righe NOME=valore
    int virtualControls; // 0 automatico (nascosti con un controller), 1 sempre, 2 mai
    String padMap;       // "azione=tasto,..." (sdl_xinput.cpp), solo le voci cambiate
    boolean padSwap;     // levette scambiate

    /** comandi del pad che il gioco vede (uscite) e nomi mostrati; l'ordine e' quello della pagina di mappatura */
    static final String[] PAD_ACTIONS = { "rt", "lt", "a", "b", "x", "y", "rb", "lb", "rs", "ls", "up", "down", "left", "right", "start", "back" };
    static final String[] PAD_ACTION_LABELS = { "Fire", "Aim down sights", "Jump", "Crouch / prone", "Reload / use", "Switch weapon",
        "Lethal grenade", "Tactical grenade", "Melee (knife)", "Sprint", "D-pad up", "D-pad down", "D-pad left", "D-pad right",
        "Pause menu", "Scoreboard" };
    static final String[] PAD_BUTTONS = { "a", "b", "x", "y", "lb", "rb", "lt", "rt", "ls", "rs", "start", "back", "up", "down", "left", "right" };
    static final String[] PAD_BUTTON_LABELS = { "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "L3", "R3", "Start", "Select",
        "D-pad up", "D-pad down", "D-pad left", "D-pad right" };

    static String buttonLabel(String b) {
        for (int i = 0; i < PAD_BUTTONS.length; ++i)
            if (PAD_BUTTONS[i].equals(b))
                return PAD_BUTTON_LABELS[i];
        return b;
    }

    /** tasto fisico assegnato a un'azione (predefinito: lo stesso nome) */
    String padButtonFor(String action) {
        if (padMap != null)
            for (String e : padMap.split(","))
                if (e.startsWith(action + "="))
                    return e.substring(action.length() + 1);
        return action;
    }

    void setPadButton(String action, String button) {
        StringBuilder sb = new StringBuilder();
        if (padMap != null)
            for (String e : padMap.split(","))
                if (!e.isEmpty() && !e.startsWith(action + "="))
                    sb.append(sb.length() > 0 ? "," : "").append(e);
        if (!action.equals(button))
            sb.append(sb.length() > 0 ? "," : "").append(action).append('=').append(button);
        padMap = sb.toString();
    }

    static File root() {
        return new File(Environment.getExternalStorageDirectory(), "T5Jool");
    }

    static File logsDir() {
        return new File(root(), "logs");
    }

    static File driversDir(Context ctx) {
        return new File(ctx.getFilesDir(), "drivers");
    }

    static File marker(Context ctx) {
        return new File(ctx.getFilesDir(), "game_running");
    }

    static T5Settings load(Context ctx) {
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        T5Settings s = new T5Settings();
        s.gameDir = p.getString("gameDir", new File(root(), "game").getAbsolutePath());
        s.driver = p.getString("driver", "");
        s.resolution = p.getInt("resolution", 100);
        s.renderHeight = p.getInt("renderHeight", 0);
        s.quality = p.getInt("quality", 1);
        s.upscaler = p.getInt("upscaler", 0);
        s.postAa = p.getInt("postAa", 0);
        s.lsfg = p.getBoolean("lsfg", false);
        s.lsfgMultiplier = p.getInt("lsfgMultiplier", 0);
        s.lsfgFlow = p.getInt("lsfgFlow", 1);
        s.lsfgPerformance = p.getBoolean("lsfgPerformance", true);
        s.sharpness = p.getInt("sharpness", 200);
        s.fps = p.getInt("fps", 60);
        s.showFps = p.getBoolean("showFps", false);
        s.showPing = p.getBoolean("showPing", true);
        s.vsync = p.getBoolean("vsync", false);
        s.logLevel = p.getInt("logLevel", 1);
        s.rumble = p.getBoolean("rumble", true);
        s.rumbleStrength = p.getInt("rumbleStrength", 70);
        s.gyro = p.getInt("gyro", 0);
        s.gyroSens = p.getInt("gyroSens", 150);
        s.gyroInvertX = p.getBoolean("gyroInvertX", false);
        s.gyroInvertY = p.getBoolean("gyroInvertY", false);
        s.dxvkHud = p.getBoolean("dxvkHud", false);
        s.dxvkHudItems = p.getString("dxvkHudItems", s.dxvkHudItems);
        s.dxvkHudScale = p.getInt("dxvkHudScale", 1);
        s.touch = p.getBoolean("touch", true);
        s.touchOpacity = p.getInt("touchOpacity", 45);
        s.touchScale = p.getInt("touchScale", 100);
        s.touchLook = p.getInt("touchLook", 100);
        s.touchSmooth = Math.max(0, Math.min(100, p.getInt("touchSmooth", 50)));
        s.fov = Math.max(65, Math.min(110, p.getInt("fov", 65)));
        s.extraArgs = p.getString("extraArgs", "");
        s.playerName = p.getString("playerName", "Player");
        s.mod = p.getString("mod", "");
        s.envVars = p.getString("envVars", "");
        s.virtualControls = p.getInt("virtualControls", 0);
        s.padMap = p.getString("padMap", "");
        s.padSwap = p.getBoolean("padSwap", false);
        return s;
    }

    void save(Context ctx) {
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putString("gameDir", gameDir)
            .putString("driver", driver)
            .putInt("resolution", resolution)
            .putInt("renderHeight", renderHeight)
            .putInt("quality", quality)
            .putInt("upscaler", upscaler)
            .putInt("postAa", postAa)
            .putBoolean("lsfg", lsfg)
            .putInt("lsfgMultiplier", lsfgMultiplier)
            .putInt("lsfgFlow", lsfgFlow)
            .putBoolean("lsfgPerformance", lsfgPerformance)
            .putInt("sharpness", sharpness)
            .putInt("fps", fps)
            .putBoolean("showFps", showFps)
            .putBoolean("showPing", showPing)
            .putBoolean("vsync", vsync)
            .putInt("logLevel", logLevel)
            .putBoolean("rumble", rumble)
            .putInt("rumbleStrength", rumbleStrength)
            .putInt("gyro", gyro)
            .putInt("gyroSens", gyroSens)
            .putBoolean("gyroInvertX", gyroInvertX)
            .putBoolean("gyroInvertY", gyroInvertY)
            .putBoolean("dxvkHud", dxvkHud)
            .putString("dxvkHudItems", dxvkHudItems)
            .putInt("dxvkHudScale", dxvkHudScale)
            .putBoolean("touch", touch)
            .putInt("touchOpacity", touchOpacity)
            .putInt("touchScale", touchScale)
            .putInt("touchLook", touchLook)
            .putInt("fov", fov)
            .putInt("touchSmooth", touchSmooth)
            .putString("extraArgs", extraArgs)
            .putString("playerName", playerName)
            .putString("mod", mod)
            .putString("envVars", envVars)
            .putInt("virtualControls", virtualControls)
            .putString("padMap", padMap)
            .putBoolean("padSwap", padSwap)
            .commit();
    }

    /** A mod folder: has a mod.ff (made with the Black Ops mod tools). 0.8's own helper folders are not mods. */
    static boolean isMod(File dir) {
        String n = dir.getName();
        return dir.isDirectory() && !n.startsWith("_") && !n.equalsIgnoreCase("t5save") && new File(dir, "mod.ff").isFile();
    }

    /** Mods in <game>/mods, sorted by name. */
    static List<String> mods(String gameDir) {
        List<String> out = new ArrayList<>();
        File[] list = gameDir == null ? null : new File(gameDir, "mods").listFiles();
        if (list != null)
            for (File f : list)
                if (isMod(f))
                    out.add(f.getName());
        java.util.Collections.sort(out, String.CASE_INSENSITIVE_ORDER);
        return out;
    }

    /** Riga di comando del gioco: prima le opzioni --t5-* (bo1_android.cpp), poi quelle del motore. */
    List<String> arguments(Context ctx) {
        return arguments(ctx, null);
    }

    /** map: mappa da avviare subito (es. "zombietron"), null = menu principale. */
    List<String> arguments(Context ctx, String map) {
        return arguments(ctx, map, null);
    }

    /** cmd: comandi del gioco da eseguire all'avvio, separati da ';' (riavvio per la mappa successiva). */
    List<String> arguments(Context ctx, String map, String cmd) {
        List<String> a = new ArrayList<>();
        a.add("--t5-home=" + root().getAbsolutePath());
        a.add("--t5-libdir=" + ctx.getApplicationInfo().nativeLibraryDir);
        // cache degli shader nella memoria interna dell'app (files/, non cache/: Android puo' svuotare cache/ quando manca spazio)
        a.add("--t5-cache=" + new File(ctx.getFilesDir(), "shadercache").getAbsolutePath());
        a.add("--t5-marker=" + marker(ctx).getAbsolutePath());
        a.add("--t5-env=BO1_GAME_DIR=" + gameDir);
        if (driver != null && !driver.isEmpty())
            a.add("--t5-driver=" + new File(driversDir(ctx), driver).getAbsolutePath());
        if (dxvkHud) {
            String items = dxvkHudItems == null || dxvkHudItems.isEmpty() ? "fps" : dxvkHudItems;
            int sc = Math.max(0, Math.min(dxvkHudScale, HUD_SCALES.length - 1));
            if (sc != 1)
                items += ",scale=" + HUD_SCALES[sc];
            a.add("--t5-env=DXVK_HUD=" + items);
        }
        float scale = renderScale(ctx);
        if (scale < 0.99f) {
            // il gioco disegna a questa scala e DXVK ingrandisce allo schermo con l'upscaler
            a.add("--t5-env=T5_RENDER_SCALE=" + String.format(java.util.Locale.US, "%.5f", scale));
            a.add("--t5-env=T5_UPSCALER=" + UPSCALER_VALUES[Math.max(0, Math.min(upscaler, UPSCALER_VALUES.length - 1))]);
            a.add("--t5-env=T5_SHARPNESS=" + (sharpness / 100.0f));
        }
        if (postAa == 1)
            a.add("--t5-env=T5_POSTAA=fxaa");
        if (lsfg && lsfgDll(ctx).isFile()) {
            a.add("--t5-env=T5_LSFG=1");
            a.add("--t5-env=LSFG_LEGACY=1");
            a.add("--t5-env=LSFG_DLL_PATH=" + lsfgDll(ctx).getAbsolutePath());
            a.add("--t5-env=LSFG_MULTIPLIER=" + LSFG_MULT_VALUES[Math.max(0, Math.min(lsfgMultiplier, 2))]);
            a.add("--t5-env=LSFG_FLOW_SCALE=" + LSFG_FLOW_VALUES[Math.max(0, Math.min(lsfgFlow, 2))]);
            a.add("--t5-env=LSFG_PERFORMANCE_MODE=" + (lsfgPerformance ? "1" : "0"));
            a.add("--t5-env=TMPDIR=" + ctx.getCacheDir().getAbsolutePath());
            // file di configurazione di lsfg-vk in una cartella scrivibile: senza, cercava /etc/lsfg-vk (sola lettura
            // su Android), non riusciva a crearlo e si spegneva ("IGNORING")
            a.add("--t5-env=LSFG_CONFIG=" + new File(ctx.getFilesDir(), "lsfg/conf.toml").getAbsolutePath());
        }
        if (padMap != null && !padMap.isEmpty())
            a.add("--t5-env=T5_PADMAP=" + padMap);
        if (padSwap)
            a.add("--t5-env=T5_PADSWAP=1");
        if (envVars != null)
            for (String line : envVars.split("[\\n;]"))
                if (line.trim().contains("="))
                    a.add("--t5-env=" + line.trim());

        set(a, "fs_b", gameDir);
        set(a, "fs_h", new File(root(), "home").getAbsolutePath());
        // log: game.txt (stderr) ha gia' tutta la console; console_mp.log solo nel livello dettagliato, senza flush per riga
        a.add("--t5-env=T5_LOG=" + logLevel);
        set(a, "logfile", logLevel >= 2 ? "1" : "0");
        set(a, "net_ip", "0.0.0.0"); // rete locale: si puo' ospitare e collegarsi ad altri ("connect <ip>")
        set(a, "r_fullscreen", "1");
        set(a, "gpad_enabled", "1");
        set(a, "com_maxfps", String.valueOf(fps));
        // 0.9: the engine's frame limit is nx_fpsLimit (60 by default); com_maxfps alone was ignored
        set(a, "nx_fpsLimit", String.valueOf(fps));
        set(a, "cg_drawFPS", showFps ? "1" : "0"); // 0.9: the game's own counter works in every mode
        set(a, "r_vsync", vsync ? "1" : "0");
        set(a, "bo1_touch", virtualControls == 2 ? "0" : "1");
        set(a, "bo1_touch_autohide", virtualControls == 1 ? "0" : "1");
        if (playerName != null && !playerName.trim().isEmpty())
            set(a, "name", playerName.trim());
        // mod: the engine loads mods/<name>/mod.kbz (converted by the launcher next to mod.ff) and its files
        if (mod != null && !mod.isEmpty() && isMod(new File(new File(gameDir, "mods"), mod)))
            set(a, "fs_game", "mods/" + mod);
        set(a, "bo1_touch_opacity", String.valueOf(touchOpacity / 100.0f));
        set(a, "bo1_touch_scale", String.valueOf(touchScale / 100.0f));
        set(a, "bo1_touch_look", String.valueOf(touchLook / 100.0f));
        set(a, "bo1_touch_smooth", String.valueOf(touchSmooth / 100.0f));
        if (fov > 65)
            set(a, "cg_fov_default", String.valueOf(fov));
        set(a, "bo1_rumble", rumble ? "1" : "0");
        set(a, "bo1_rumble_strength", String.valueOf(rumbleStrength / 100.0f));
        // 0.9: the engine's motion aim (pad gyro, or the device's own on Android)
        set(a, "nx_gyroAim", gyro == 0 ? "0" : String.valueOf(gyroSens / 100.0f));
        set(a, "nx_gyroAlways", gyro == 1 ? "1" : "0");
        set(a, "nx_gyroInvertYaw", gyroInvertX ? "1" : "0");
        set(a, "nx_gyroInvertPitch", gyroInvertY ? "1" : "0");
        // senza r_picmip_manual il gioco sceglie da solo il dettaglio delle texture e ignora r_picmip
        set(a, "r_picmip_manual", "1");
        switch (quality) {
            case 3: // Potato: tutto al minimo, per i dispositivi deboli
                set(a, "r_picmip", "3"); set(a, "r_picmip_bump", "3"); set(a, "r_picmip_spec", "3");
                set(a, "sm_enable", "0"); set(a, "r_dof_enable", "0"); set(a, "r_glow", "0"); set(a, "r_distortion", "0");
                set(a, "fx_marks", "0"); set(a, "fx_drawClouds", "0"); set(a, "r_aaSamples", "1");
                set(a, "r_texFilterAnisoMax", "1"); set(a, "r_lodScaleRigid", "2"); set(a, "r_lodScaleSkinned", "2");
                set(a, "r_dlightLimit", "1"); set(a, "cg_brass", "0");
                break;
            case 0:
                set(a, "r_picmip", "2"); set(a, "sm_enable", "0"); set(a, "r_dof_enable", "0");
                set(a, "r_glow", "0"); set(a, "r_distortion", "0"); set(a, "fx_marks", "0"); set(a, "r_aaSamples", "1");
                break;
            case 2:
                set(a, "r_picmip", "0"); set(a, "sm_enable", "1"); set(a, "r_dof_enable", "1");
                set(a, "r_glow", "1"); set(a, "r_distortion", "1"); set(a, "fx_marks", "1");
                break;
            default:
                set(a, "r_picmip", "1"); set(a, "sm_enable", "0"); set(a, "r_dof_enable", "0");
                set(a, "r_glow", "1"); set(a, "r_distortion", "1"); set(a, "fx_marks", "1"); set(a, "r_aaSamples", "1");
                break;
        }
        if (map != null && !map.isEmpty()) {
            a.add("+devmap");
            a.add(map);
        }
        if (cmd != null)
            for (String part : cmd.split(";")) {
                String[] words = part.trim().split("\\s+");
                if (words.length == 0 || words[0].isEmpty())
                    continue;
                a.add("+" + words[0]);
                for (int w = 1; w < words.length; ++w)
                    a.add(words[w]);
            }
        if (extraArgs != null && !extraArgs.trim().isEmpty())
            for (String s : extraArgs.trim().split("\\s+"))
                a.add(s);
        return a;
    }

    /** dimensioni reali dello schermo in orizzontale: {larghezza, altezza} */
    static int[] screenSize(Context ctx) {
        android.util.DisplayMetrics dm = new android.util.DisplayMetrics();
        android.view.WindowManager wm = (android.view.WindowManager) ctx.getSystemService(Context.WINDOW_SERVICE);
        wm.getDefaultDisplay().getRealMetrics(dm);
        return new int[] { Math.max(dm.widthPixels, dm.heightPixels), Math.min(dm.widthPixels, dm.heightPixels) };
    }

    /** altezze di rendering proposte (16:9 comuni) non piu' grandi dello schermo, la prima e' la nativa */
    static int[] renderHeights(Context ctx) {
        int nativeH = screenSize(ctx)[1];
        int[] common = { 1440, 1200, 1080, 900, 810, 720, 630, 540, 480 };
        List<Integer> out = new ArrayList<>();
        out.add(nativeH);
        for (int h : common)
            if (h < nativeH && h * 100 >= nativeH * 40)
                out.add(h);
        int[] r = new int[out.size()];
        for (int i = 0; i < r.length; ++i)
            r[i] = out.get(i);
        return r;
    }

    float renderScale(Context ctx) {
        int nativeH = screenSize(ctx)[1];
        if (renderHeight > 0 && nativeH > 0)
            return Math.min(1.0f, renderHeight / (float) nativeH);
        return resolution > 0 && resolution < 100 ? resolution / 100.0f : 1.0f;
    }

    private static void set(List<String> a, String dvar, String value) {
        a.add("+set");
        a.add(dvar);
        a.add(value);
    }
}

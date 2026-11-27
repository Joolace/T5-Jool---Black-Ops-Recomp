package com.joolace.t5jool;

import android.content.Intent;
import android.os.Bundle;
import android.os.Process;
import android.util.DisplayMetrics;
import android.util.Log;

import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.util.List;

/**
 * The game: SDLActivity with libmain.so (Campaign and Zombies) or libmain_mp.so (Multiplayer), extra "mp".
 * Gira nel processo ":game"; quando l'attivita' finisce il processo termina (il motore non riparte due volte).
 */
public class GameActivity extends SDLActivity {
    private static final String TAG = "T5Jool";
    private T5Settings mSettings;
    private LoadingView mLoading;

    /** Extra: true = Multiplayer (libmain_mp.so), false = Campaign and Zombies (libmain.so). */
    static final String EXTRA_MP = "mp";

    private String gameLibrary() {
        return getIntent().getBooleanExtra(EXTRA_MP, false) ? "main_mp" : "main";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        mSettings = T5Settings.load(this);
        // "partita in corso": il gioco lo cancella all'uscita pulita; se resta, il lanciatore salva i log del crash
        try {
            new FileOutputStream(T5Settings.marker(this)).close();
        } catch (Exception e) {
            Log.w(TAG, "marker: " + e);
        }
        super.onCreate(savedInstanceState);
        applyResolution();
        // schermata di caricamento sopra il nero dell'avvio del motore, finche' il gioco disegna (gameReady)
        String cmd = getIntent().getStringExtra("cmd");
        String map = getIntent().getStringExtra("map");
        mLoading = LoadingView.show(this, cmd != null ? cmd : map != null ? "map " + map : null, cmd != null);
        getWindow().getDecorView().postDelayed(this::gameReady, 90000); // al massimo 90 s, qualunque cosa succeda
    }

    /** Chiamato dal gioco (bo1_android_game_ready) quando ha mostrato i primi frame: via la schermata di caricamento. */
    public void gameReady() {
        runOnUiThread(() -> {
            if (mLoading != null) {
                mLoading.hide();
                mLoading = null;
            }
        });
    }

    @Override
    public void loadLibraries() {
        SDL.loadLibrary("SDL2", this);
        // T5Jool 0.9: a native 64-bit engine, loaded like any library (no more low-address loader)
        SDL.loadLibrary(gameLibrary(), this);
    }

    @Override
    protected String getMainSharedObject() {
        return getApplicationInfo().nativeLibraryDir + "/lib" + gameLibrary() + ".so";
    }

    @Override
    protected String[] getArguments() {
        List<String> args = mSettings.arguments(this, getIntent().getStringExtra("map"), getIntent().getStringExtra("cmd"));
        Log.i(TAG, "argomenti: " + args);
        return args.toArray(new String[0]);
    }

    /** Scala della risoluzione: la superficie si rende piu' piccola e il compositore la ingrandisce. */
    private void applyResolution() {
        // T5Jool: non piu' usato. La scala di rendering la fa il gioco (T5_RENDER_SCALE) e DXVK ingrandisce con
        // l'upscaler: una superficie piu' piccola (setFixedSize) qui non veniva ingrandita dal compositore.
        if (true)
            return;
        DisplayMetrics dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(dm);
        int w = Math.max(dm.widthPixels, dm.heightPixels) * mSettings.resolution / 100;
        int h = Math.min(dm.widthPixels, dm.heightPixels) * mSettings.resolution / 100;
        mSurface.getHolder().setFixedSize(w & ~1, h & ~1);
        Log.i(TAG, "risoluzione " + mSettings.resolution + "%: " + w + "x" + h);
    }

    /**
     * Chiamato dal gioco (bo1_android_relaunch) prima di una nuova mappa: il motore ha bisogno di memoria sotto i 2 GB
     * in pezzi grandi, e dopo una partita e' frammentata. Il lanciatore riapre il gioco in un processo nuovo che esegue cmd.
     */
    public void relaunchGame(String cmd) {
        Intent i = new Intent(this, LauncherActivity.class);
        i.putExtra(LauncherActivity.EXTRA_RELAUNCH, cmd);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        startActivity(i);
        runOnUiThread(this::finish);
    }

    /** T5Jool: Back (or the pad B that Android turns into Back) never closes the game: SDL already gives it
     *  to the game as Escape. Leaving is Quit in the game's menu, or Home. */
    @Override
    public void onBackPressed() {
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // il motore non si puo' riavviare nello stesso processo
        Process.killProcess(Process.myPid());
    }
}

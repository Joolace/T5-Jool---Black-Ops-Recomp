package com.joolace.t5jool;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageInstaller;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import org.json.JSONArray;
import org.json.JSONObject;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/**
 * Lanciatore di T5Jool: schermata iniziale (Zombies / Multiplayer) e impostazioni a schede, navigabili
 * anche con il controller (d-pad + A). Controlla file e permessi, raccoglie impostazioni e log; se la
 * partita precedente e' crashata salva logcat.
 */
public class LauncherActivity extends Activity {
    private static final int REQ_DRIVER = 1;
    private static final int REQ_FOLDER = 2;
    private static final int REQ_LSFG_DLL = 3;

    // tema: nero e rosso
    private static final int BG = Color.rgb(10, 10, 12);
    private static final int CARD = Color.rgb(28, 20, 20);
    private static final int ACCENT = Color.rgb(222, 52, 40);
    private static final int ACCENT_DIM = Color.argb(150, 222, 52, 40);
    private static final int TEXT = Color.WHITE;
    private static final int TEXT_DIM = Color.rgb(170, 160, 160);

    private T5Settings mSettings;
    private boolean mInSettings;
    private String mCrashNotice;
    private int mSettingsScroll;

    static final String EXTRA_RELAUNCH = "t5_relaunch";
    /** T5Jool 0.9: the next play() starts Multiplayer (libmain_mp.so) instead of Campaign and Zombies. */
    private boolean mPlayMp;
    private String mRelaunchCmd;
    private LoadingView mRelaunchLoading;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mSettings = T5Settings.load(this);
        getWindow().getDecorView().setBackgroundColor(BG);
        mRelaunchCmd = getIntent().getStringExtra(EXTRA_RELAUNCH);
        // animazione di avvio: solo all'apertura dell'app (non al ritorno dal gioco o al riavvio per una nuova mappa)
        if (savedInstanceState == null && mRelaunchCmd == null) {
            getWindow().getDecorView().post(() -> SplashView.show(this, null));
            getWindow().getDecorView().postDelayed(() -> checkUpdates(false), 4500); // dopo l'animazione
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        String cmd = intent.getStringExtra(EXTRA_RELAUNCH);
        if (cmd != null)
            mRelaunchCmd = cmd;
    }

    /** Il gioco ha chiesto di ripartire per la mappa successiva: si aspetta che il suo processo sia finito e lo si riapre. */
    private void relaunchWhenGameExited(String cmd, int tries) {
        boolean alive = false;
        ActivityManager am = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        if (am != null && am.getRunningAppProcesses() != null)
            for (ActivityManager.RunningAppProcessInfo p : am.getRunningAppProcesses())
                if (p.processName != null && p.processName.endsWith(":game"))
                    alive = true;
        if (alive && tries < 40) {
            new Handler(Looper.getMainLooper()).postDelayed(() -> relaunchWhenGameExited(cmd, tries + 1), 100);
            return;
        }
        Intent i = new Intent(this, GameActivity.class);
        // T5Jool 0.9: "+t5mode mp|sp" = the engine's SP <-> MP button (NX_ChainLoadSibling)
        if (cmd != null && cmd.startsWith("+t5mode ")) {
            i.putExtra(GameActivity.EXTRA_MP, cmd.endsWith("mp"));
        } else {
            i.putExtra("cmd", cmd);
        }
        startActivity(i);
        // il gioco ha la sua schermata uguale: questa si toglie quando il lanciatore non si vede piu'
        final LoadingView v = mRelaunchLoading;
        mRelaunchLoading = null;
        if (v != null)
            new Handler(Looper.getMainLooper()).postDelayed(v::hide, 1500);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mRelaunchCmd != null) {
            String cmd = mRelaunchCmd;
            mRelaunchCmd = null;
            getIntent().removeExtra(EXTRA_RELAUNCH);
            showHome();
            // tra la chiusura del gioco e la riapertura: la schermata di caricamento, non il lanciatore
            mRelaunchLoading = LoadingView.show(this, cmd, true);
            relaunchWhenGameExited(cmd, 0);
            return;
        }
        checkPreviousCrash();
        if (mInMapping)
            showMapping();
        else if (mInSettings)
            showSettings();
        else
            showHome();
    }

    @Override
    public void onBackPressed() {
        if (mTouchLayout != null) {
            mTouchLayout.save();
            leaveTouchLayout();
        } else if (mInMapping) {
            mInMapping = false;
            mSettings.save(this);
            showSettings();
        } else if (mInSettings) {
            mSettings.save(this);
            showHome();
        } else {
            super.onBackPressed();
        }
    }

    // ------------------------------------------------------------------------------------- stile
    private int dp(float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()));
    }

    private GradientDrawable box(int fill, int stroke, int strokeDp, int radiusDp) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(fill);
        d.setCornerRadius(dp(radiusDp));
        if (strokeDp > 0)
            d.setStroke(dp(strokeDp), stroke);
        return d;
    }

    /** sfondo di un elemento selezionabile: bordo che si accende con il focus (controller) o il tocco */
    private StateListDrawable selectable(int fill, int radiusDp) {
        StateListDrawable s = new StateListDrawable();
        s.addState(new int[] { android.R.attr.state_pressed }, box(fill, Color.WHITE, 2, radiusDp));
        s.addState(new int[] { android.R.attr.state_focused }, box(fill, Color.WHITE, 3, radiusDp));
        s.addState(new int[] {}, box(fill, ACCENT_DIM, 1, radiusDp));
        return s;
    }

    private TextView text(String s, float sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        if (bold)
            t.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD));
        return t;
    }

    /**
     * Pad dopo il touch: toccando una voce Android entra in "modalita' touch" e, ricostruita la pagina, nessuna voce
     * ha piu' il fuoco; i tasti del pad/della console non facevano nulla. Al primo tasto si esce dalla modalita'
     * touch e si mette il fuoco sulla prima voce visibile.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (e.getAction() == KeyEvent.ACTION_DOWN && isPadKey(e.getKeyCode())) {
            View f = getCurrentFocus();
            if (f == null || !f.isShown() || f.isInTouchMode()) {
                View target = (f != null && f.isShown()) ? f : firstFocusable(getWindow().getDecorView());
                if (target != null) {
                    target.requestFocusFromTouch();
                    return true;
                }
            }
        }
        return super.dispatchKeyEvent(e);
    }

    private static boolean isPadKey(int k) {
        switch (k) {
            case KeyEvent.KEYCODE_DPAD_UP: case KeyEvent.KEYCODE_DPAD_DOWN: case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT: case KeyEvent.KEYCODE_DPAD_CENTER: case KeyEvent.KEYCODE_BUTTON_A:
            case KeyEvent.KEYCODE_BUTTON_X: case KeyEvent.KEYCODE_BUTTON_Y: case KeyEvent.KEYCODE_BUTTON_L1:
            case KeyEvent.KEYCODE_BUTTON_R1: case KeyEvent.KEYCODE_ENTER:
                return true;
            default:
                return false;
        }
    }

    private static View firstFocusable(View v) {
        if (v == null || v.getVisibility() != View.VISIBLE)
            return null;
        if (v.isFocusable() && v.isEnabled() && v.isShown() && !(v instanceof ViewGroup && ((ViewGroup) v).getChildCount() > 0 && !v.isClickable()))
            return v;
        if (v instanceof ViewGroup) {
            ViewGroup g = (ViewGroup) v;
            for (int i = 0; i < g.getChildCount(); ++i) {
                View r = firstFocusable(g.getChildAt(i));
                if (r != null)
                    return r;
            }
        }
        return null;
    }

    private TouchLayoutView mTouchLayout;

    /** editor della disposizione dei comandi touch, a tutto schermo come in partita */
    private void showTouchLayout() {
        mSettings.save(this);
        mTouchLayout = new TouchLayoutView(this, mSettings.touchScale / 100.0f, () -> {
            toast("Touch layout saved");
            leaveTouchLayout();
        });
        setContentView(mTouchLayout);
        // a tutto schermo come in partita: le posizioni sono frazioni dello schermo
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    private void leaveTouchLayout() {
        getWindow().getDecorView().setSystemUiVisibility(0);
        mTouchLayout = null;
        showSettings();
    }

    static final String DISCORD_URL = "https://discord.gg/rcMeKZcShx";

    private void openDiscord() {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(DISCORD_URL)));
        } catch (Exception e) {
            toast("Discord: " + DISCORD_URL);
        }
    }

    private View outlinedButton(String label, View.OnClickListener l) {
        TextView b = text(label, 15, ACCENT, true);
        b.setAllCaps(true);
        b.setLetterSpacing(0.08f);
        b.setGravity(Gravity.CENTER);
        b.setPadding(dp(18), dp(10), dp(18), dp(10));
        b.setBackground(selectable(Color.TRANSPARENT, 8));
        b.setFocusable(true);
        b.setClickable(true);
        b.setOnClickListener(l);
        return b;
    }

    // ------------------------------------------------------------------------------------- home
    private void showHome() {
        mInSettings = false;
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(BG);

        // sfondo: l'immagine dello sfondo personale (o di Zombies) a tutto schermo, scurita verso sinistra
        Bitmap bgImage = customBackground();
        if (bgImage == null)
            bgImage = drawable("card_zombies");
        if (bgImage != null) {
            ImageView iv = new ImageView(this);
            iv.setImageBitmap(bgImage);
            iv.setScaleType(ImageView.ScaleType.CENTER_CROP);
            iv.setAlpha(0.55f);
            root.addView(iv, new FrameLayout.LayoutParams(-1, -1));
        }
        View shade = new View(this);
        shade.setBackground(new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
            new int[] { Color.argb(250, 8, 8, 10), Color.argb(215, 8, 8, 10), Color.argb(120, 8, 8, 10) }));
        root.addView(shade, new FrameLayout.LayoutParams(-1, -1));
        View redGlow = new View(this);
        redGlow.setBackground(new GradientDrawable(GradientDrawable.Orientation.BOTTOM_TOP,
            new int[] { Color.argb(90, 150, 20, 16), Color.TRANSPARENT }));
        root.addView(redGlow, new FrameLayout.LayoutParams(-1, dp(160), Gravity.BOTTOM));

        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.HORIZONTAL);
        content.setPadding(dp(32), dp(24), dp(32), dp(20));
        root.addView(content, new FrameLayout.LayoutParams(-1, -1));

        // ---- colonna sinistra: marchio, Gioca, Dead Ops, pulsanti piccoli
        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);
        content.addView(left, new LinearLayout.LayoutParams(0, -1, 11));

        LinearLayout brand = new LinearLayout(this);
        brand.setGravity(Gravity.CENTER_VERTICAL);
        Bitmap logo = drawable("logo_t5");
        if (logo != null) {
            ImageView lv = new ImageView(this);
            lv.setImageBitmap(logo);
            lv.setAdjustViewBounds(true);
            LinearLayout.LayoutParams llp = new LinearLayout.LayoutParams(-2, dp(58));
            llp.rightMargin = dp(16);
            brand.addView(lv, llp);
        }
        LinearLayout titles = new LinearLayout(this);
        titles.setOrientation(LinearLayout.VERTICAL);
        LinearLayout titleRow = new LinearLayout(this);
        titleRow.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = text("T5JOOL", 34, TEXT, true);
        title.setLetterSpacing(0.08f);
        titleRow.addView(title);
        TextView pill = text(("ALPHA " + versionName().replace("-alpha", "")).toUpperCase(Locale.US), 10, TEXT, true);
        pill.setLetterSpacing(0.08f);
        pill.setPadding(dp(9), dp(3), dp(9), dp(3));
        pill.setBackground(box(ACCENT, ACCENT, 0, 20));
        LinearLayout.LayoutParams plp = new LinearLayout.LayoutParams(-2, -2);
        plp.leftMargin = dp(12);
        titleRow.addView(pill, plp);
        titles.addView(titleRow);
        titles.addView(text("Black Ops Recomp", 14, Color.rgb(225, 200, 196), false));
        brand.addView(titles);
        left.addView(brand);

        // modalita': due schede grandi affiancate (riempiono lo spazio libero), Dead Ops sotto in piccolo
        LinearLayout modes = new LinearLayout(this);
        LinearLayout.LayoutParams mlp = new LinearLayout.LayoutParams(-1, 0, 1);
        mlp.topMargin = dp(20);
        mlp.bottomMargin = dp(12);
        left.addView(modes, mlp);
        Bitmap zmImage = userImage("card_zombies");
        View playBtn = modeCard("CAMPAIGN & ZOMBIES", "The story missions and every Zombies map you own", zmImage != null ? zmImage : drawable("card_zombies"),
            Color.rgb(90, 24, 20), IconView.PLAY, v -> { mPlayMp = false; play(null, null); });
        LinearLayout.LayoutParams zlp = new LinearLayout.LayoutParams(0, -1, 1);
        zlp.rightMargin = dp(12);
        modes.addView(playBtn, zlp);
        // il gioco si apre sulla pagina Multiplayer del menu (bo1_menu_start, ui/bo1_menu.cpp)
        Bitmap mpImage = userImage("card_mp");
        modes.addView(modeCard("MULTIPLAYER", "Matches with bots, Host Match and the server browser", mpImage != null ? mpImage : drawable("card_mp"), Color.rgb(70, 26, 22), IconView.PLAY,
            v -> { mPlayMp = true; play(null, null); }), new LinearLayout.LayoutParams(0, -1, 1));

        LinearLayout small = new LinearLayout(this);
        small.addView(smallButton(IconView.DRIVER, "Settings", v -> showSettings()), smallLp(true));
        small.addView(smallButton(IconView.BUG, "Report bug", v -> reportBug(false)), smallLp(true));
        small.addView(smallButton(IconView.DISCORD, "Discord", v -> openDiscord()), smallLp(false));
        left.addView(small, buttonLp(dp(12)));
        TextView foot = text("Fan project, not affiliated with Activision or Treyarch. Uses your own copy of the game.", 11,
            Color.rgb(130, 120, 120), false);
        left.addView(foot);

        // ---- colonna destra: stato e scorciatoie
        ScrollView rightScroll = new ScrollView(this);
        LinearLayout right = new LinearLayout(this);
        right.setOrientation(LinearLayout.VERTICAL);
        right.setGravity(Gravity.BOTTOM);
        rightScroll.addView(right);
        rightScroll.setFillViewport(true);
        LinearLayout.LayoutParams rlp = new LinearLayout.LayoutParams(0, -1, 6);
        rlp.leftMargin = dp(32);
        content.addView(rightScroll, rlp);
        right.addView(new View(this), new LinearLayout.LayoutParams(-1, 0, 1));

        if (mCrashNotice != null)
            right.addView(infoCard(IconView.BUG, "LAST GAME CRASHED", mCrashNotice, Color.rgb(255, 130, 100),
                v -> reportBug(true)), infoLp());
        String problem = gameFilesProblem();
        right.addView(infoCard(IconView.STORAGE, "GAME FILES", problem != null ? problem : "Ready to play",
            problem != null ? Color.rgb(255, 200, 90) : Color.rgb(120, 220, 120), v -> openSettings(0)), infoLp());
        // grafica e driver in una sola scheda, per lasciare spazio fra le schede
        right.addView(infoCard(IconView.SCREEN, "GRAPHICS", graphicsSummary() + "\nDriver: "
            + (mSettings.driver.isEmpty() ? "system" : mSettings.driver), TEXT, v -> openSettings(1)), infoLp());

        setContentView(root);
        playBtn.requestFocus();
    }

    private void openSettings(int category) {
        mCategory = category;
        mSettingsScroll = 0;
        showSettings();
    }

    /** "1600 x 900  -  60 FPS  -  FXAA  -  Frame gen 2x" */
    private String graphicsSummary() {
        int[] screen = T5Settings.screenSize(this);
        int h = Math.round(mSettings.renderScale(this) * screen[1]);
        int w = (Math.round(screen[0] * h / (float) Math.max(1, screen[1])) + 1) & ~1;
        StringBuilder s = new StringBuilder();
        s.append(w).append(" x ").append(h).append("  -  ");
        s.append(mSettings.fps == 0 ? "Unlimited FPS" : mSettings.fps + " FPS");
        s.append("  -  ").append(T5Settings.QUALITY_LABELS[Math.max(0, Math.min(3, mSettings.quality))]).append(" quality");
        if (mSettings.postAa == 1)
            s.append("  -  FXAA");
        if (mSettings.lsfg && T5Settings.lsfgDll(this).isFile())
            s.append("\nFrame generation ").append(T5Settings.LSFG_MULT_LABELS[Math.max(0, Math.min(2, mSettings.lsfgMultiplier))]);
        return s.toString();
    }

    private LinearLayout.LayoutParams buttonLp(int bottom) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = bottom;
        return lp;
    }

    private LinearLayout.LayoutParams smallLp(boolean gap) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, -2, 1);
        lp.rightMargin = gap ? dp(10) : 0;
        return lp;
    }

    private LinearLayout.LayoutParams infoLp() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(12);
        return lp;
    }

    /** pulsante grande con icona: pieno (rosso) per Gioca, scuro con il bordo per gli altri */
    private View bigButton(int icon, String title, String subtitle, boolean primary, View.OnClickListener l) {
        LinearLayout b = new LinearLayout(this);
        b.setGravity(Gravity.CENTER_VERTICAL);
        b.setPadding(dp(20), dp(primary ? 18 : 14), dp(20), dp(primary ? 18 : 14));
        StateListDrawable bg = new StateListDrawable();
        int fill = primary ? ACCENT : Color.argb(200, 28, 20, 20);
        int focusFill = primary ? Color.rgb(240, 70, 56) : Color.argb(230, 60, 26, 24);
        bg.addState(new int[] { android.R.attr.state_focused }, box(focusFill, Color.WHITE, 3, 14));
        bg.addState(new int[] { android.R.attr.state_pressed }, box(focusFill, Color.WHITE, 2, 14));
        bg.addState(new int[] {}, box(fill, primary ? fill : ACCENT_DIM, 1, 14));
        b.setBackground(bg);
        b.setFocusable(true);
        b.setClickable(true);
        b.setOnClickListener(l);
        IconView iv = new IconView(this, icon, primary ? Color.WHITE : ACCENT);
        LinearLayout.LayoutParams ilp = new LinearLayout.LayoutParams(dp(primary ? 34 : 28), dp(primary ? 34 : 28));
        ilp.rightMargin = dp(16);
        b.addView(iv, ilp);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        TextView t = text(title, primary ? 24 : 18, TEXT, true);
        t.setLetterSpacing(0.06f);
        col.addView(t);
        col.addView(text(subtitle, 12, primary ? Color.rgb(255, 225, 220) : TEXT_DIM, false));
        b.addView(col, new LinearLayout.LayoutParams(0, -2, 1));
        b.addView(text("›", 30, primary ? Color.WHITE : ACCENT, true));
        return b;
    }

    private View smallButton(int icon, String label, View.OnClickListener l) {
        LinearLayout b = new LinearLayout(this);
        b.setGravity(Gravity.CENTER);
        b.setPadding(dp(10), dp(10), dp(10), dp(10));
        b.setBackground(selectable(Color.argb(200, 28, 20, 20), 12));
        b.setFocusable(true);
        b.setClickable(true);
        b.setOnClickListener(l);
        boolean discord = icon == IconView.DISCORD;
        IconView iv = new IconView(this, icon, discord ? Color.rgb(88, 101, 242) : ACCENT);
        LinearLayout.LayoutParams ilp = new LinearLayout.LayoutParams(dp(22), dp(22));
        ilp.rightMargin = dp(8);
        b.addView(iv, ilp);
        TextView lt = text(label, 13, TEXT, true);
        lt.setSingleLine(true); // su schermi stretti si accorcia con "..." invece di andare a capo
        lt.setEllipsize(android.text.TextUtils.TruncateAt.END);
        b.addView(lt);
        return b;
    }

    /** scheda di stato a destra: icona, etichetta, valore; si tocca per andare alla voce giusta */
    private View infoCard(int icon, String label, String value, int valueColor, View.OnClickListener l) {
        LinearLayout c = new LinearLayout(this);
        c.setGravity(Gravity.CENTER_VERTICAL);
        c.setPadding(dp(18), dp(14), dp(16), dp(14));
        c.setBackground(selectable(Color.argb(190, 20, 14, 14), 12));
        c.setFocusable(true);
        c.setClickable(true);
        c.setOnClickListener(l);
        IconView iv = new IconView(this, icon, ACCENT);
        LinearLayout.LayoutParams ilp = new LinearLayout.LayoutParams(dp(26), dp(26));
        ilp.rightMargin = dp(14);
        c.addView(iv, ilp);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        TextView lt = text(label, 10, TEXT_DIM, true);
        lt.setLetterSpacing(0.12f);
        col.addView(lt);
        TextView vt = text(value, 13, valueColor, false);
        vt.setPadding(0, dp(2), 0, 0);
        col.addView(vt);
        c.addView(col, new LinearLayout.LayoutParams(0, -2, 1));
        c.addView(text("›", 22, ACCENT, true));
        return c;
    }

    private View modeCard(String title, String subtitle, Bitmap image, int tint, int icon, View.OnClickListener l) {
        FrameLayout card = new FrameLayout(this);
        card.setFocusable(true);
        card.setClickable(true);
        card.setOnClickListener(l);
        card.setClipToOutline(true);
        StateListDrawable fg = new StateListDrawable();
        fg.addState(new int[] { android.R.attr.state_focused }, box(Color.TRANSPARENT, Color.WHITE, 3, 14));
        fg.addState(new int[] { android.R.attr.state_pressed }, box(Color.argb(50, 255, 255, 255), Color.WHITE, 2, 14));
        fg.addState(new int[] {}, box(Color.TRANSPARENT, ACCENT_DIM, 1, 14));
        card.setForeground(fg);
        GradientDrawable bg = new GradientDrawable(GradientDrawable.Orientation.TL_BR, new int[] { tint, Color.rgb(12, 10, 12) });
        bg.setCornerRadius(dp(14));
        card.setBackground(bg);
        if (image != null) {
            ImageView iv = new ImageView(this);
            iv.setImageBitmap(image);
            iv.setScaleType(ImageView.ScaleType.CENTER_CROP);
            card.addView(iv, new FrameLayout.LayoutParams(-1, -1));
        } else {
            // senza immagine: icona grande e tenue al centro
            IconView big = new IconView(this, icon, Color.argb(70, 255, 255, 255));
            card.addView(big, new FrameLayout.LayoutParams(dp(64), dp(64), Gravity.CENTER));
        }
        // sfumatura in basso per leggere il titolo
        View shade = new View(this);
        shade.setBackground(new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
            new int[] { Color.TRANSPARENT, Color.argb(220, 0, 0, 0) }));
        FrameLayout.LayoutParams slp = new FrameLayout.LayoutParams(-1, -1);
        card.addView(shade, slp);

        LinearLayout labels = new LinearLayout(this);
        labels.setOrientation(LinearLayout.VERTICAL);
        labels.setPadding(dp(18), 0, dp(16), dp(16));
        TextView t = text(title, 22, TEXT, true);
        t.setLetterSpacing(0.06f);
        t.setShadowLayer(8, 0, 2, Color.BLACK);
        labels.addView(t);
        TextView st = text(subtitle, 12, TEXT_DIM, false);
        labels.addView(st);
        FrameLayout.LayoutParams llp = new FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM);
        card.addView(labels, llp);
        return card;
    }

    /** immagine delle schede (res/drawable-nodpi) */
    private Bitmap drawable(String name) {
        try {
            int id = getResources().getIdentifier(name, "drawable", getPackageName());
            return id != 0 ? BitmapFactory.decodeResource(getResources(), id) : null;
        } catch (Throwable t) {
            return null;
        }
    }

    /** sfondo, musica e anteprime delle mappe del menu del gioco (assets/media) in <home>/t5jool/, se cambiati */
    private void copyMenuMedia() {
        copyMediaDir("media", new File(T5Settings.root(), "home/t5jool"));
    }

    /** quando e' stato installato/aggiornato l'APK (i file copiati prima vanno rinnovati) */
    private long apkTime() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime;
        } catch (Exception e) {
            return 0;
        }
    }

    private void copyMediaDir(String path, File dir) {
        AssetManager am = getAssets();
        try {
            String[] list = am.list(path);
            if (list == null)
                return;
            dir.mkdirs();
            for (String name : list) {
                String child = path + "/" + name;
                String[] sub = am.list(child);
                if (sub != null && sub.length > 0) {
                    copyMediaDir(child, new File(dir, name));
                    continue;
                }
                File dest = new File(dir, name);
                long size = -1;
                try (android.content.res.AssetFileDescriptor fd = am.openFd(child)) {
                    size = fd.getLength();
                } catch (Exception ignored) {
                }
                // stessa dimensione e copiato dopo l'installazione dell'APK: e' gia' aggiornato
                if (dest.isFile() && (size < 0 || dest.length() == size) && dest.lastModified() >= apkTime())
                    continue;
                try (InputStream in = am.open(child); OutputStream out = new FileOutputStream(dest)) {
                    copy(in, out);
                }
            }
        } catch (Exception e) {
            android.util.Log.w("T5Jool", "copia media " + path + ": " + e);
        }
    }

    /** lo sfondo personale del menu (menu_bg.bmp), se c'e', anche come immagine della scheda Zombies */
    /** immagine scelta dall'utente per una scheda: <cartella T5Jool>/<nome>.jpg|.png|.webp (anche in home/t5jool/).
     *  Le immagini del gioco non sono nell'APK: chi vuole la copertina la mette qui. */
    private Bitmap userImage(String name) {
        String[] exts = { ".jpg", ".png", ".webp" };
        File[] dirs = { T5Settings.root(), new File(T5Settings.root(), "home/t5jool") };
        for (File d : dirs)
            for (String e : exts) {
                File f = new File(d, name + e);
                if (!f.isFile())
                    continue;
                try {
                    BitmapFactory.Options o = new BitmapFactory.Options();
                    o.inSampleSize = f.length() > 3_000_000 ? 2 : 1;
                    Bitmap b = BitmapFactory.decodeFile(f.getAbsolutePath(), o);
                    if (b != null)
                        return b;
                } catch (Throwable ignored) {
                }
            }
        return null;
    }

    private Bitmap customBackground() {
        File[] candidates = { new File(T5Settings.root(), "menu_bg.bmp"), new File(T5Settings.root(), "home/t5jool/menu_bg.bmp") };
        for (File f : candidates) {
            if (f.isFile()) {
                try {
                    BitmapFactory.Options o = new BitmapFactory.Options();
                    o.inSampleSize = 1;
                    Bitmap b = BitmapFactory.decodeFile(f.getAbsolutePath(), o);
                    if (b != null)
                        return b;
                } catch (Throwable ignored) {
                }
            }
        }
        return null;
    }

    // --------------------------------------------------------------------------------- impostazioni
    // Due colonne: a sinistra le categorie, a destra le voci della categoria scelta. Ogni voce ha il nome, una riga che
    // spiega a cosa serve e il valore attuale; le scelte con poche opzioni sono pulsanti affiancati (niente finestre).
    private LinearLayout mList;
    private int mCategory;
    private int mFocusCategory = -1; // dopo aver cambiato categoria il fuoco resta sul suo pulsante

    private static final String[] CATEGORIES = { "Game", "Graphics", "Frame generation", "Controls", "Touch and gyro",
        "Advanced", "Help and about" };
    private static final String[] CATEGORY_HINTS = { "Game files, name, driver", "Resolution, quality, FPS",
        "Lossless Scaling", "Controller and vibration", "On-screen buttons, gyro", "Logs, command line", "Bugs, updates, info" };

    private void showSettings() {
        mInSettings = true;
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(24), dp(14), dp(24), 0);

        LinearLayout top = new LinearLayout(this);
        top.setGravity(Gravity.CENTER_VERTICAL);
        View back = outlinedButton("Back", v -> { mSettings.save(this); showHome(); });
        top.addView(back);
        TextView title = text("SETTINGS", 24, TEXT, true);
        title.setPadding(dp(18), 0, 0, 0);
        top.addView(title);
        TextView saved = text("Changes are saved automatically", 12, TEXT_DIM, false);
        saved.setGravity(Gravity.END);
        top.addView(saved, new LinearLayout.LayoutParams(0, -2, 1));
        root.addView(top);
        View line = new View(this);
        line.setBackgroundColor(ACCENT);
        LinearLayout.LayoutParams llp = new LinearLayout.LayoutParams(-1, dp(2));
        llp.topMargin = dp(10);
        root.addView(line, llp);

        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.HORIZONTAL);
        root.addView(body, new LinearLayout.LayoutParams(-1, 0, 1));

        // colonna delle categorie
        ScrollView sideScroll = new ScrollView(this);
        LinearLayout side = new LinearLayout(this);
        side.setOrientation(LinearLayout.VERTICAL);
        side.setPadding(0, dp(10), dp(14), dp(20));
        sideScroll.addView(side);
        body.addView(sideScroll, new LinearLayout.LayoutParams(dp(230), -1));
        View firstFocus = back;
        for (int i = 0; i < CATEGORIES.length; ++i) {
            final int index = i;
            final boolean sel = i == mCategory;
            LinearLayout b = new LinearLayout(this);
            b.setOrientation(LinearLayout.VERTICAL);
            b.setPadding(dp(16), dp(10), dp(12), dp(10));
            b.setBackground(selectable(sel ? Color.rgb(60, 22, 20) : BG, 10));
            b.setFocusable(true);
            b.setClickable(true);
            TextView name = text(CATEGORIES[i], 16, sel ? TEXT : TEXT_DIM, true);
            b.addView(name);
            b.addView(text(CATEGORY_HINTS[i], 11, sel ? Color.rgb(230, 170, 165) : Color.rgb(120, 112, 112), false));
            b.setOnClickListener(v -> {
                if (mCategory != index) {
                    mCategory = index;
                    mSettingsScroll = 0;
                }
                mFocusCategory = index;
                showSettings();
            });
            LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(-1, -2);
            blp.bottomMargin = dp(6);
            side.addView(b, blp);
            if (i == (mFocusCategory >= 0 ? mFocusCategory : mCategory))
                firstFocus = b;
        }

        // voci della categoria
        final ScrollView scroll = new ScrollView(this);
        mList = new LinearLayout(this);
        mList.setOrientation(LinearLayout.VERTICAL);
        mList.setPadding(dp(6), dp(4), dp(4), dp(28));
        scroll.addView(mList);
        body.addView(scroll, new LinearLayout.LayoutParams(0, -1, 1));

        buildSettings();
        setContentView(root);
        scroll.post(() -> scroll.scrollTo(0, mSettingsScroll));
        scroll.getViewTreeObserver().addOnScrollChangedListener(() -> mSettingsScroll = scroll.getScrollY());
        firstFocus.requestFocus();
        mFocusCategory = -1;
    }

    private void section(String name) {
        section(name, null);
    }

    private void section(String name, String desc) {
        TextView t = text(name, 13, ACCENT, true);
        t.setLetterSpacing(0.1f);
        t.setPadding(dp(4), dp(16), 0, desc != null ? dp(2) : dp(6));
        mList.addView(t);
        if (desc != null) {
            TextView d = text(desc, 12, TEXT_DIM, false);
            d.setPadding(dp(4), 0, 0, dp(8));
            mList.addView(d);
        }
    }

    /** scheda con titolo, valore e (opzionale) controllo a destra */
    private LinearLayout card(String title, String value, View right, View.OnClickListener click) {
        return card(title, null, value, right, click);
    }

    /** scheda: titolo, descrizione (grigia), valore (rosso, sotto) e un controllo a destra */
    private LinearLayout card(String title, String desc, String value, View right, View.OnClickListener click) {
        LinearLayout c = new LinearLayout(this);
        c.setGravity(Gravity.CENTER_VERTICAL);
        c.setPadding(dp(18), dp(12), dp(16), dp(12));
        c.setBackground(selectable(CARD, 12));
        LinearLayout labels = new LinearLayout(this);
        labels.setOrientation(LinearLayout.VERTICAL);
        labels.addView(text(title, 15, TEXT, true));
        if (desc != null) {
            TextView d = text(desc, 12, TEXT_DIM, false);
            d.setPadding(0, dp(2), dp(8), 0);
            labels.addView(d);
        }
        if (value != null) {
            TextView v = text(value, 14, Color.rgb(255, 120, 105), false);
            v.setPadding(0, dp(4), 0, 0);
            labels.addView(v);
        }
        c.addView(labels, new LinearLayout.LayoutParams(0, -2, 1));
        if (right != null)
            c.addView(right);
        if (click != null) {
            c.setFocusable(true);
            c.setClickable(true);
            c.setOnClickListener(click);
        }
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(8);
        mList.addView(c, lp);
        return c;
    }

    /** riquadro informativo (non selezionabile) */
    private void note(String s) {
        TextView t = text(s, 12, TEXT_DIM, false);
        t.setPadding(dp(16), dp(10), dp(16), dp(10));
        t.setBackground(box(Color.rgb(20, 16, 16), Color.argb(70, 255, 255, 255), 1, 10));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(8);
        mList.addView(t, lp);
    }

    private TextView chevron() {
        return text("›", 26, ACCENT, true);
    }

    private interface IntSetter { void set(int v); }

    /** tema dei dialoghi: scuro con le scelte nel rosso dell'app (res/values/themes.xml) */
    private int dialogTheme() {
        int id = getResources().getIdentifier("T5JoolDialog", "style", getPackageName());
        return id != 0 ? id : android.R.style.Theme_DeviceDefault_Dialog_Alert;
    }

    /** interruttori rossi come il resto dell'app (il verde/grigio di sistema non c'entrava) */
    private static void tintSwitch(Switch sw) {
        int[][] states = { { android.R.attr.state_checked }, { } };
        sw.setThumbTintList(new android.content.res.ColorStateList(states,
            new int[] { ACCENT, Color.rgb(150, 140, 140) }));
        sw.setTrackTintList(new android.content.res.ColorStateList(states,
            new int[] { Color.argb(140, 222, 52, 40), Color.argb(90, 255, 255, 255) }));
    }

    private void toggle(String title, boolean value, IntSetter setter) {
        toggle(title, null, value, setter);
    }

    private void toggle(String title, String desc, boolean value, IntSetter setter) {
        final Switch sw = new Switch(this);
        tintSwitch(sw);
        sw.setChecked(value);
        sw.setClickable(false);
        sw.setFocusable(false);
        card(title, desc, null, sw, v -> {
            sw.setChecked(!sw.isChecked());
            setter.set(sw.isChecked() ? 1 : 0);
            mSettings.save(this);
        });
    }

    private void choice(String title, String[] labels, int selected, IntSetter setter) {
        choice(title, null, labels, selected, setter);
    }

    /** fino a 4 opzioni brevi: pulsanti affiancati nella scheda; altrimenti una lista in una finestra */
    private void choice(String title, String desc, String[] labels, int selected, IntSetter setter) {
        final int sel = Math.max(0, Math.min(selected, labels.length - 1));
        boolean shortLabels = labels.length <= 4;
        for (String l : labels)
            if (l.length() > 14)
                shortLabels = false;
        if (!shortLabels) {
            card(title, desc, labels[sel], chevron(), v ->
                new AlertDialog.Builder(this, dialogTheme())
                    .setTitle(title)
                    .setSingleChoiceItems(labels, sel, (d, which) -> {
                        setter.set(which);
                        mSettings.save(this);
                        d.dismiss();
                        showSettings();
                    })
                    .show());
            return;
        }
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setPadding(dp(18), dp(12), dp(16), dp(12));
        c.setBackground(box(CARD, ACCENT_DIM, 1, 12));
        c.addView(text(title, 15, TEXT, true));
        if (desc != null) {
            TextView d = text(desc, 12, TEXT_DIM, false);
            d.setPadding(0, dp(2), 0, 0);
            c.addView(d);
        }
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(10), 0, 0);
        for (int i = 0; i < labels.length; ++i) {
            final int which = i;
            TextView b = text(labels[i], 14, i == sel ? TEXT : TEXT_DIM, i == sel);
            b.setGravity(Gravity.CENTER);
            b.setPadding(dp(14), dp(9), dp(14), dp(9));
            StateListDrawable bg = new StateListDrawable();
            int fill = i == sel ? ACCENT : Color.rgb(40, 30, 30);
            bg.addState(new int[] { android.R.attr.state_focused }, box(fill, Color.WHITE, 3, 8));
            bg.addState(new int[] { android.R.attr.state_pressed }, box(fill, Color.WHITE, 2, 8));
            bg.addState(new int[] {}, box(fill, Color.argb(60, 255, 255, 255), 1, 8));
            b.setBackground(bg);
            b.setFocusable(true);
            b.setClickable(true);
            b.setOnClickListener(v -> {
                setter.set(which);
                mSettings.save(this);
                showSettings();
            });
            LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(0, -2, 1);
            blp.rightMargin = i < labels.length - 1 ? dp(8) : 0;
            row.addView(b, blp);
        }
        c.addView(row);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(8);
        mList.addView(c, lp);
    }

    private void slider(String title, int min, int max, int value, String unit, IntSetter setter) {
        slider(title, null, min, max, value, unit, setter);
    }

    private void slider(String title, String desc, int min, int max, int value, String unit, IntSetter setter) {
        LinearLayout c = card(title, desc, null, null, null);
        final TextView label = text(value + unit, 14, TEXT, false);
        label.setGravity(Gravity.END);
        SeekBar sb = new SeekBar(this);
        sb.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));
        sb.setThumbTintList(android.content.res.ColorStateList.valueOf(ACCENT));
        sb.setProgressBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.argb(110, 255, 255, 255)));
        sb.setMax(max - min);
        sb.setProgress(value - min);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean user) { label.setText((p + min) + unit); setter.set(p + min); }
            @Override public void onStartTrackingTouch(SeekBar s) { }
            @Override public void onStopTrackingTouch(SeekBar s) { mSettings.save(LauncherActivity.this); }
        });
        c.addView(sb, new LinearLayout.LayoutParams(dp(240), -2));
        c.addView(label, new LinearLayout.LayoutParams(dp(56), -2));
    }

    /** voci dell'overlay di DXVK: scelta multipla */
    private void hudItemsCard() {
        final String[] keys = T5Settings.HUD_KEYS;
        java.util.List<String> on = java.util.Arrays.asList(mSettings.dxvkHudItems.split(","));
        StringBuilder summary = new StringBuilder();
        final boolean[] checked = new boolean[keys.length];
        for (int i = 0; i < keys.length; ++i) {
            checked[i] = on.contains(keys[i]);
            if (checked[i])
                summary.append(summary.length() > 0 ? ", " : "").append(T5Settings.HUD_LABELS[i]);
        }
        card("Overlay items", "What the DXVK overlay shows", summary.length() > 0 ? summary.toString() : "FPS", chevron(), v ->
            new AlertDialog.Builder(this, dialogTheme())
                .setTitle("DXVK overlay items")
                .setMultiChoiceItems(T5Settings.HUD_LABELS, checked, (d, which, isChecked) -> checked[which] = isChecked)
                .setPositiveButton("OK", (d, w) -> {
                    StringBuilder sb = new StringBuilder();
                    for (int i = 0; i < keys.length; ++i)
                        if (checked[i])
                            sb.append(sb.length() > 0 ? "," : "").append(keys[i]);
                    mSettings.dxvkHudItems = sb.length() > 0 ? sb.toString() : "fps";
                    mSettings.save(this);
                    showSettings();
                })
                .setNegativeButton("Cancel", null)
                .show());
    }

    private interface TextSetter { void set(String v); }

    private void textField(String title, String value, String hint, boolean multiline, TextSetter setter) {
        textField(title, null, value, hint, multiline, setter);
    }

    private void textField(String title, String desc, String value, String hint, boolean multiline, TextSetter setter) {
        card(title, desc, value == null || value.isEmpty() ? hint : value, chevron(), v -> {
            final EditText e = new EditText(this);
            e.setText(value);
            e.setTextColor(TEXT);
            e.setHint(hint);
            if (!multiline)
                e.setSingleLine(true);
            else
                e.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
            new AlertDialog.Builder(this, dialogTheme())
                .setTitle(title)
                .setView(e)
                .setPositiveButton("OK", (d, w) -> { setter.set(e.getText().toString()); mSettings.save(this); showSettings(); })
                .setNegativeButton("Cancel", null)
                .show();
        });
    }

    // ------------------------------------------------------------------------------- preset grafici
    private static final String[] PRESET_LABELS = { "Potato", "Battery", "Balanced", "Quality", "Smooth" };
    private static final String[] PRESET_HINTS = {
        "540p, low quality, 30 FPS, no extras: for weak devices, as light as it gets",
        "720p, low quality, 30 FPS: cooler and longer battery",
        "About 900p, medium quality, 60 FPS, FXAA: the best mix for most devices",
        "Native resolution, high quality, 60 FPS, FXAA",
        "720p, medium quality, 120 FPS: for high refresh rate screens" };

    /** altezza di rendering piu' vicina a target tra quelle proposte (0 = nativa) */
    private int nearestHeight(int target) {
        int[] heights = T5Settings.renderHeights(this);
        if (target <= 0 || heights.length == 0 || target >= heights[0])
            return 0;
        int best = heights[0];
        for (int h : heights)
            if (Math.abs(h - target) < Math.abs(best - target))
                best = h;
        return best == heights[0] ? 0 : best;
    }

    private void applyPreset(int p) {
        int[] screen = T5Settings.screenSize(this);
        int h;
        switch (p) {
            case 0: h = nearestHeight(540); mSettings.quality = 3; mSettings.fps = 30; mSettings.postAa = 0; break;
            case 1: h = nearestHeight(720); mSettings.quality = 0; mSettings.fps = 30; mSettings.postAa = 0; break;
            case 2: h = nearestHeight(900); mSettings.quality = 1; mSettings.fps = 60; mSettings.postAa = 1; break;
            case 3: h = 0; mSettings.quality = 2; mSettings.fps = 60; mSettings.postAa = 1; break;
            default: h = nearestHeight(720); mSettings.quality = 1; mSettings.fps = 120; mSettings.postAa = 1; break;
        }
        mSettings.renderHeight = h;
        mSettings.resolution = h == 0 || screen[1] <= 0 ? 100 : Math.round(h * 100f / screen[1]);
        mSettings.upscaler = 0;
        if (p == 0) { // Potato: anche le scritte a schermo che costano qualcosa
            mSettings.dxvkHud = false;
            mSettings.lsfg = false;
        }
        mSettings.save(this);
        toast(PRESET_LABELS[p] + " preset applied");
        showSettings();
    }

    // ------------------------------------------------------------------------------- voci per categoria
    private void buildSettings() {
        switch (mCategory) {
            case 0: buildGame(); break;
            case 1: buildGraphics(); break;
            case 2: buildFrameGen(); break;
            case 3: buildControls(); break;
            case 4: buildTouch(); break;
            case 5: buildAdvanced(); break;
            default: buildAbout(); break;
        }
    }

    private void buildGame() {
        section("GAME FILES", "T5Jool needs your own copy of Call of Duty: Black Ops for PC");
        String folderProblem = gameFilesProblem();
        card("Game folder", folderProblem != null ? "Problem: " + folderProblem : "Ready: main/ and zone/ found",
            mSettings.gameDir, chevron(), v -> new AlertDialog.Builder(this, dialogTheme())
                .setTitle("Game folder")
                .setMessage("Folder with the Black Ops (PC) files: main/ and zone/.")
                .setPositiveButton("Choose folder", (d, w) -> pickFolder())
                .setNeutralButton("Type path", (d, w) -> typeFolder())
                .setNegativeButton("Cancel", null)
                .show());
        if (!hasStorage())
            card("Storage access", "Needed to read the game files", "Not granted: tap to allow", chevron(), v -> requestStorage());
        section("PLAYER");
        textField("Player name", "Shown to the other players in co-op", mSettings.playerName, "Player", false,
            v -> mSettings.playerName = v.trim());
        section("MODS", "Community mods made with the Black Ops mod tools: put each one in mods/<name>/ in the game folder");
        {
            List<String> mods = T5Settings.mods(mSettings.gameDir);
            String[] labels = new String[mods.size() + 1];
            labels[0] = "None (base game)";
            int sel = 0;
            for (int i = 0; i < mods.size(); ++i) {
                labels[i + 1] = mods.get(i);
                if (mods.get(i).equals(mSettings.mod))
                    sel = i + 1;
            }
            if (mods.isEmpty())
                card("Mod", "No mods found in " + new File(mSettings.gameDir, "mods").getPath(), "None", null, null);
            else
                choice("Mod", "The game starts with this mod. To play a mod online everyone needs the same one", labels, sel,
                    which -> mSettings.mod = which == 0 ? "" : mods.get(which - 1));
        }
        section("VIEW");
        slider("Field of view", "Wider view in game. At 65 the game's own setting is used", 65, 110, mSettings.fov, "\u00b0",
            v -> mSettings.fov = v);

        section("GRAPHICS DRIVER", "Turnip (Mesa) is usually faster and more compatible than the system driver on Adreno");
        choice("Vulkan driver", "Driver used by the game", driverLabels(), driverIndex(), which -> {
            List<String> names = driverNames();
            mSettings.driver = which == 0 || which > names.size() ? "" : names.get(which - 1);
            if (mSettings.driver.isEmpty()) // scelto a mano: niente piu' richiesta di scaricare Turnip
                updatePrefs().edit().putBoolean("trySystemDriver", true).apply();
        });
        card("Download a driver", isAdreno() ? "Get a tested Turnip driver from GitHub and install it automatically"
            : "Experimental: get the PanVK driver for Mali GPUs from GitHub", null, chevron(), v -> chooseDriverDownload(null));
        card("Import a driver", "A Turnip / Adreno driver .zip with meta.json", null, chevron(), v -> importDriver());
        if (!mSettings.driver.isEmpty())
            card("Delete selected driver", null, mSettings.driver, chevron(), v -> deleteDriver());
    }

    private void buildGraphics() {
        section("QUICK PRESETS", "One tap sets resolution, quality, FPS limit and antialiasing. You can fine-tune below");
        for (int i = 0; i < PRESET_LABELS.length; ++i) {
            final int p = i;
            card(PRESET_LABELS[i], PRESET_HINTS[i], null, chevron(), v -> applyPreset(p));
        }

        section("IMAGE");
        {
            // risoluzioni vere (1920x1080, 1600x900, 1280x720...) calcolate dallo schermo
            int[] screen = T5Settings.screenSize(this);
            int[] heights = T5Settings.renderHeights(this);
            String[] labels = new String[heights.length];
            int selected = 0;
            int current = Math.round(mSettings.renderScale(this) * screen[1]);
            for (int i = 0; i < heights.length; ++i) {
                int w = (Math.round(screen[0] * heights[i] / (float) screen[1]) + 1) & ~1;
                labels[i] = w + " x " + heights[i] + (i == 0 ? "  (native)" : "  (" + Math.round(heights[i] * 100f / screen[1]) + "%)");
                if (Math.abs(heights[i] - current) < Math.abs(heights[selected] - current))
                    selected = i;
            }
            choice("Render resolution", "Lower = more FPS and less heat; the image is upscaled to the screen", labels,
                selected, which -> {
                    mSettings.renderHeight = which == 0 ? 0 : heights[which];
                    mSettings.resolution = which == 0 ? 100 : Math.round(heights[which] * 100f / screen[1]);
                });
        }
        if (mSettings.renderScale(this) < 0.99f) {
            choice("Upscaler", "How the lower resolution is enlarged to the screen", T5Settings.UPSCALER_LABELS,
                mSettings.upscaler, which -> mSettings.upscaler = which);
            if (mSettings.upscaler == 0)
                choice("Upscaler sharpness", "Higher = sharper, but more shimmering edges", T5Settings.SHARP_LABELS,
                    indexOf(T5Settings.SHARP_VALUES, mSettings.sharpness), which -> mSettings.sharpness = T5Settings.SHARP_VALUES[which]);
        }
        choice("Antialiasing", "FXAA smooths jagged edges (about 1 ms per frame); it takes the place of the upscaler",
            new String[] { "Off", "FXAA" }, mSettings.postAa, which -> mSettings.postAa = which);
        choice("Graphics quality", "Texture detail, shadows and effects", T5Settings.QUALITY_LABELS, mSettings.quality,
            which -> mSettings.quality = which);

        section("SMOOTHNESS");
        choice("FPS limit", "A lower limit keeps the device cooler and the frame rate steadier", T5Settings.FPS_LABELS,
            indexOf(T5Settings.FPS_VALUES, mSettings.fps), which -> mSettings.fps = T5Settings.FPS_VALUES[which]);
        toggle("V-Sync", "Off = less input lag (the FPS limit still applies)", mSettings.vsync, v -> mSettings.vsync = v != 0);

        section("ON-SCREEN INFO");
        toggle("Show FPS", "Frame counter at the top left", mSettings.showFps, v -> mSettings.showFps = v != 0);
        toggle("DXVK overlay", "Detailed graphics stats (GPU load, frame times...)", mSettings.dxvkHud,
            v -> { mSettings.dxvkHud = v != 0; showSettings(); });
        if (mSettings.dxvkHud) {
            hudItemsCard();
            choice("Overlay size", null, T5Settings.HUD_SCALE_LABELS, mSettings.dxvkHudScale, which -> mSettings.dxvkHudScale = which);
        }
    }

    private void buildFrameGen() {
        section("LOSSLESS SCALING FRAME GENERATION", "Experimental. Adds generated frames between the real ones");
        note("Smoother motion with a little more input lag. It also uses the GPU, so the real frame rate can drop a bit: "
            + "it works best when the game already reaches its FPS limit. You need your own copy of Lossless Scaling "
            + "(Steam): import its Lossless.dll once, it stays in the app's private storage.");
        boolean hasDll = T5Settings.lsfgDll(this).isFile();
        card("Lossless.dll", "Steam/steamapps/common/Lossless Scaling/Lossless.dll",
            hasDll ? "Imported" : "Not imported: tap to pick it", chevron(), v -> {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                startActivityForResult(i, REQ_LSFG_DLL);
            });
        if (hasDll) {
            toggle("Frame generation", "Show FPS then shows both numbers, e.g. 60 FPS -> 120 LSFG", mSettings.lsfg,
                v -> { mSettings.lsfg = v != 0; showSettings(); });
            if (mSettings.lsfg) {
                choice("Multiplier", "Frames on screen for each real frame", T5Settings.LSFG_MULT_LABELS,
                    mSettings.lsfgMultiplier, which -> mSettings.lsfgMultiplier = which);
                choice("Flow scale", "Lower = lighter on the GPU, slightly less precise", T5Settings.LSFG_FLOW_LABELS,
                    mSettings.lsfgFlow, which -> mSettings.lsfgFlow = which);
                toggle("Performance mode", "A lighter model: faster, slightly lower quality", mSettings.lsfgPerformance,
                    v -> mSettings.lsfgPerformance = v != 0);
            }
        }
    }

    private void buildControls() {
        section("CONTROLLER");
        card("Connected controller", null, connectedController(), null, null);
        card("Button mapping", "Choose which button does what", mSettings.padMap == null || mSettings.padMap.isEmpty()
            ? "Default" : "Custom", chevron(), v -> showMapping());
        toggle("Swap sticks", "Left-handed: move with the right stick, look with the left", mSettings.padSwap,
            v -> mSettings.padSwap = v != 0);
        section("VIBRATION");
        toggle("Vibration", "Controller rumble, or the device's motor without a controller", mSettings.rumble,
            v -> mSettings.rumble = v != 0);
        slider("Vibration strength", null, 10, 100, mSettings.rumbleStrength, "%", v -> mSettings.rumbleStrength = v);
    }

    private void buildTouch() {
        section("ON-SCREEN CONTROLS");
        choice("Virtual controls", "When the touch buttons are shown",
            new String[] { "Automatic (hidden with a controller)", "Always visible", "Off" },
            mSettings.virtualControls, which -> mSettings.virtualControls = which);
        card("Edit layout", "Move and resize the on-screen buttons (also in game: Pause > Touch controls layout)", null,
            chevron(), v -> showTouchLayout());
        slider("Opacity", null, 10, 100, mSettings.touchOpacity, "%", v -> mSettings.touchOpacity = v);
        slider("Size", null, 60, 160, mSettings.touchScale, "%", v -> mSettings.touchScale = v);
        slider("Look speed", "How fast the camera turns when you drag", 20, 400, mSettings.touchLook, "%",
            v -> mSettings.touchLook = v);
        slider("Aim smoothing", "Higher is smoother, lower reacts faster. 0 turns it off", 0, 100, mSettings.touchSmooth, "%",
            v -> mSettings.touchSmooth = v);
        section("GYRO AIMING", "Aim by moving the device");
        choice("Gyro aiming", null, T5Settings.GYRO_LABELS, Math.max(0, Math.min(2, mSettings.gyro)), which -> mSettings.gyro = which);
        if (mSettings.gyro != 0) {
            slider("Gyro sensitivity", null, 20, 500, mSettings.gyroSens, "%", v -> mSettings.gyroSens = v);
            toggle("Invert horizontal", null, mSettings.gyroInvertX, v -> mSettings.gyroInvertX = v != 0);
            toggle("Invert vertical", null, mSettings.gyroInvertY, v -> mSettings.gyroInvertY = v != 0);
        }
    }

    private void buildAdvanced() {
        section("ADVANCED", "Only change these if you know what they do");
        textField("Extra command line", "Passed to the game at start", mSettings.extraArgs, "None (e.g. +set developer 1)",
            false, v -> mSettings.extraArgs = v.trim());
        textField("Environment variables", "One NAME=value per line, for DXVK, Turnip or the game", mSettings.envVars,
            "None (e.g. TU_DEBUG=...)", true, v -> mSettings.envVars = v.trim());
        section("LOGS");
        choice("Game log detail", "More detail helps bug reports but costs a little performance", T5Settings.LOG_LABELS,
            Math.max(0, Math.min(2, mSettings.logLevel)), which -> mSettings.logLevel = which);
        card("Export logs to Download", "Zip with game.txt, crashes and logcat", null, chevron(), v -> exportLogs());
        card("Save logcat now", null, T5Settings.logsDir().getAbsolutePath(), chevron(), v -> {
            File f = dumpLogcat("logcat");
            toast(f != null ? "Saved " + f.getName() : "logcat failed");
        });
    }

    private void buildAbout() {
        section("HELP");
        card("Report a problem", "Bug or crash? Saves the logs and opens a GitHub issue or Discord", null, chevron(),
            v -> reportBug(false));
        card("Check for updates", "T5Jool " + versionName() + " - also checks your Turnip driver", null, chevron(),
            v -> checkUpdates(true));

        section("ABOUT");
        aboutHero();
        section("YOUR DEVICE");
        deviceGrid();
        discordBanner();
    }

    private static final int DISCORD_BLURPLE = Color.rgb(88, 101, 242);

    /** T5Jool: logo, nome, versione in una "pillola", componenti e autore, su un fondo rosso sfumato */
    private void aboutHero() {
        LinearLayout c = new LinearLayout(this);
        c.setGravity(Gravity.CENTER_VERTICAL);
        c.setPadding(dp(22), dp(18), dp(22), dp(18));
        GradientDrawable bg = new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
            new int[] { Color.rgb(70, 16, 14), Color.rgb(28, 14, 14) });
        bg.setCornerRadius(dp(14));
        bg.setStroke(dp(1), ACCENT_DIM);
        c.setBackground(bg);
        Bitmap logo = drawable("logo_t5");
        if (logo != null) {
            ImageView lv = new ImageView(this);
            lv.setImageBitmap(logo);
            lv.setAdjustViewBounds(true);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-2, dp(72));
            lp.rightMargin = dp(18);
            c.addView(lv, lp);
        }
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        LinearLayout titleRow = new LinearLayout(this);
        titleRow.setGravity(Gravity.CENTER_VERTICAL);
        TextView name = text("T5JOOL", 28, TEXT, true);
        name.setLetterSpacing(0.08f);
        titleRow.addView(name);
        TextView pill = text(("ALPHA " + versionName().replace("-alpha", "")).toUpperCase(Locale.US), 11, TEXT, true);
        pill.setLetterSpacing(0.08f);
        pill.setPadding(dp(10), dp(3), dp(10), dp(3));
        pill.setBackground(box(ACCENT, ACCENT, 0, 20));
        LinearLayout.LayoutParams plp = new LinearLayout.LayoutParams(-2, -2);
        plp.leftMargin = dp(12);
        titleRow.addView(pill, plp);
        col.addView(titleRow);
        col.addView(text("Black Ops Recomp", 14, Color.rgb(235, 200, 196), false));
        TextView parts = text("DXVK 2.5.3  •  SDL 2.30  •  Turnip  •  lsfg-vk", 12, TEXT_DIM, false);
        parts.setPadding(0, dp(8), 0, 0);
        col.addView(parts);
        col.addView(text("Made by Joolace  •  fan project, not affiliated with Activision or Treyarch", 11,
            Color.rgb(140, 128, 128), false));
        c.addView(col, new LinearLayout.LayoutParams(0, -2, 1));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(8);
        mList.addView(c, lp);
    }

    /** nome commerciale del SoC e della GPU dal modello (Build.SOC_MODEL): "CQ8725S" -> Snapdragon 8 Elite, Adreno 830 */
    private String[] chipNames() {
        String soc = socName() == null ? "" : socName().toUpperCase(Locale.US);
        String[][] table = {
            { "CQ8725", "Snapdragon 8 Elite", "Adreno 830" }, { "SM8750", "Snapdragon 8 Elite", "Adreno 830" },
            { "SM8650", "Snapdragon 8 Gen 3", "Adreno 750" }, { "SM8635", "Snapdragon 8s Gen 3", "Adreno 735" },
            { "SM8550", "Snapdragon 8 Gen 2", "Adreno 740" }, { "QCS8550", "Snapdragon 8 Gen 2", "Adreno 740" },
            { "SM8475", "Snapdragon 8+ Gen 1", "Adreno 730" }, { "SM8450", "Snapdragon 8 Gen 1", "Adreno 730" },
            { "SM8350", "Snapdragon 888", "Adreno 660" }, { "SM8250", "Snapdragon 865", "Adreno 650" },
            { "SM7675", "Snapdragon 7+ Gen 3", "Adreno 732" }, { "SM7550", "Snapdragon 7 Gen 3", "Adreno 720" },
            { "SM7475", "Snapdragon 7+ Gen 2", "Adreno 725" }, { "SM6375", "Snapdragon 695", "Adreno 619" },
            { "SM6115", "Snapdragon 662", "Adreno 610" }, { "SM8150", "Snapdragon 855", "Adreno 640" },
        };
        for (String[] row : table)
            if (soc.startsWith(row[0]))
                return new String[] { row[1], row[2] };
        return new String[] { soc.isEmpty() ? Build.HARDWARE : soc, isAdreno() ? "Adreno" : "Mali / other" };
    }

    private View deviceTile(int icon, String label, String value) {
        LinearLayout t = new LinearLayout(this);
        t.setOrientation(LinearLayout.VERTICAL);
        t.setPadding(dp(14), dp(12), dp(12), dp(12));
        t.setBackground(box(CARD, Color.argb(60, 255, 255, 255), 1, 12));
        IconView iv = new IconView(this, icon, ACCENT);
        t.addView(iv, new LinearLayout.LayoutParams(dp(30), dp(30)));
        TextView l = text(label, 10, TEXT_DIM, true);
        l.setLetterSpacing(0.12f);
        l.setPadding(0, dp(8), 0, 0);
        t.addView(l);
        TextView v = text(value, 14, TEXT, true);
        v.setPadding(0, dp(2), 0, 0);
        t.addView(v);
        return t;
    }

    /** schede con icona: processore, GPU, RAM, sistema, schermo, driver, spazio libero */
    private void deviceGrid() {
        String[] chip = chipNames();
        int[] screen = T5Settings.screenSize(this);
        float hz = getWindowManager().getDefaultDisplay().getRefreshRate();
        long free = new File(mSettings.gameDir).getUsableSpace();
        String[][] items = {
            { "PROCESSOR", chip[0] + "\n" + socName() },
            { "GRAPHICS", chip[1] + "\n" + (mSettings.driver.isEmpty() ? "System driver" : mSettings.driver) },
            { "MEMORY", ramGb() + " RAM" },
            { "SYSTEM", "Android " + Build.VERSION.RELEASE + "\nAPI " + Build.VERSION.SDK_INT },
            { "DISPLAY", screen[0] + " x " + screen[1] + "\n" + Math.round(hz) + " Hz" },
            { "STORAGE", free > 0 ? String.format(Locale.US, "%.0f GB free", free / (1024.0 * 1024 * 1024)) : "n/a" },
        };
        int[] icons = { IconView.CPU, IconView.GPU, IconView.RAM, IconView.OS, IconView.SCREEN, IconView.STORAGE };
        TextView model = text(Build.MANUFACTURER + " " + Build.MODEL, 15, TEXT, true);
        model.setPadding(dp(4), 0, 0, dp(8));
        mList.addView(model);
        for (int r = 0; r < 2; ++r) {
            LinearLayout row = new LinearLayout(this);
            for (int i = 0; i < 3; ++i) {
                int k = r * 3 + i;
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, -1, 1);
                lp.rightMargin = i < 2 ? dp(8) : 0;
                row.addView(deviceTile(icons[k], items[k][0], items[k][1]), lp);
            }
            LinearLayout.LayoutParams rlp = new LinearLayout.LayoutParams(-1, -2);
            rlp.bottomMargin = dp(8);
            mList.addView(row, rlp);
        }
    }

    /** banner di Discord nei suoi colori, in fondo */
    private void discordBanner() {
        LinearLayout c = new LinearLayout(this);
        c.setGravity(Gravity.CENTER_VERTICAL);
        c.setPadding(dp(20), dp(16), dp(20), dp(16));
        StateListDrawable bg = new StateListDrawable();
        bg.addState(new int[] { android.R.attr.state_focused }, box(DISCORD_BLURPLE, Color.WHITE, 3, 14));
        bg.addState(new int[] { android.R.attr.state_pressed }, box(Color.rgb(71, 82, 196), Color.WHITE, 2, 14));
        bg.addState(new int[] {}, box(DISCORD_BLURPLE, DISCORD_BLURPLE, 0, 14));
        c.setBackground(bg);
        c.setFocusable(true);
        c.setClickable(true);
        c.setOnClickListener(v -> openDiscord());
        IconView logo = new IconView(this, IconView.DISCORD, Color.WHITE);
        LinearLayout.LayoutParams ilp = new LinearLayout.LayoutParams(dp(46), dp(46));
        ilp.rightMargin = dp(16);
        c.addView(logo, ilp);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.addView(text("Join us on Discord", 18, Color.WHITE, true));
        col.addView(text("News, help, bug reports and co-op partners", 12, Color.rgb(225, 228, 255), false));
        c.addView(col, new LinearLayout.LayoutParams(0, -2, 1));
        TextView go = text("JOIN", 13, DISCORD_BLURPLE, true);
        go.setLetterSpacing(0.1f);
        go.setPadding(dp(16), dp(8), dp(16), dp(8));
        go.setBackground(box(Color.WHITE, Color.WHITE, 0, 20));
        c.addView(go);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = dp(18);
        lp.bottomMargin = dp(8);
        mList.addView(c, lp);
    }

    // ---------------------------------------------------------------------------- mappatura controller
    private boolean mInMapping;

    private void showMapping() {
        mInMapping = true;
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(28), dp(16), dp(28), 0);
        LinearLayout top = new LinearLayout(this);
        top.setGravity(Gravity.CENTER_VERTICAL);
        View back = outlinedButton("Back", v -> { mInMapping = false; mSettings.save(this); showSettings(); });
        top.addView(back);
        TextView title = text("CONTROLLER MAPPING", 24, TEXT, true);
        title.setPadding(dp(18), 0, 0, 0);
        top.addView(title, new LinearLayout.LayoutParams(0, -2, 1));
        top.addView(outlinedButton("Reset", v -> { mSettings.padMap = ""; mSettings.save(this); showMapping(); }));
        root.addView(top);
        View line = new View(this);
        line.setBackgroundColor(ACCENT);
        LinearLayout.LayoutParams llp = new LinearLayout.LayoutParams(-1, dp(2));
        llp.topMargin = dp(12);
        root.addView(line, llp);
        ScrollView scroll = new ScrollView(this);
        mList = new LinearLayout(this);
        mList.setOrientation(LinearLayout.VERTICAL);
        mList.setPadding(0, dp(10), 0, dp(28));
        scroll.addView(mList);
        root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        TextView hint = text("Select an action, then press the controller button you want for it.", 13, TEXT_DIM, false);
        hint.setPadding(dp(4), 0, 0, dp(8));
        mList.addView(hint);
        for (int i = 0; i < T5Settings.PAD_ACTIONS.length; ++i) {
            final String action = T5Settings.PAD_ACTIONS[i];
            final String label = T5Settings.PAD_ACTION_LABELS[i];
            String button = mSettings.padButtonFor(action);
            TextView b = text(T5Settings.buttonLabel(button), 18, action.equals(button) ? TEXT : ACCENT, true);
            card(label, null, b, v -> captureButton(action, label));
        }
        setContentView(root);
        back.requestFocus();
    }

    private AlertDialog mCapture;
    private String mCaptureAction;

    private static String buttonFromKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A: return "a";
            case KeyEvent.KEYCODE_BUTTON_B: return "b";
            case KeyEvent.KEYCODE_BUTTON_X: return "x";
            case KeyEvent.KEYCODE_BUTTON_Y: return "y";
            case KeyEvent.KEYCODE_BUTTON_L1: return "lb";
            case KeyEvent.KEYCODE_BUTTON_R1: return "rb";
            case KeyEvent.KEYCODE_BUTTON_L2: return "lt";
            case KeyEvent.KEYCODE_BUTTON_R2: return "rt";
            case KeyEvent.KEYCODE_BUTTON_THUMBL: return "ls";
            case KeyEvent.KEYCODE_BUTTON_THUMBR: return "rs";
            case KeyEvent.KEYCODE_BUTTON_START: return "start";
            case KeyEvent.KEYCODE_BUTTON_SELECT: return "back";
            case KeyEvent.KEYCODE_DPAD_UP: return "up";
            case KeyEvent.KEYCODE_DPAD_DOWN: return "down";
            case KeyEvent.KEYCODE_DPAD_LEFT: return "left";
            case KeyEvent.KEYCODE_DPAD_RIGHT: return "right";
            default: return null;
        }
    }

    private void captureButton(String action, String label) {
        mCaptureAction = action;
        mCapture = new AlertDialog.Builder(this, dialogTheme())
            .setTitle(label)
            .setMessage("Press a controller button...\n(touch outside to cancel)")
            .setOnKeyListener((d, keyCode, e) -> {
                String b = buttonFromKey(keyCode);
                if (b == null)
                    return false;
                if (e.getAction() == KeyEvent.ACTION_UP)
                    finishCapture(b);
                return true;
            })
            .setOnDismissListener(d -> mCapture = null)
            .create();
        // grilletti analogici: arrivano come movimento, non come tasto
        mCapture.setOnShowListener(d -> mCapture.getWindow().getDecorView().setOnGenericMotionListener((v, e) -> {
            float l = Math.max(e.getAxisValue(android.view.MotionEvent.AXIS_LTRIGGER), e.getAxisValue(android.view.MotionEvent.AXIS_BRAKE));
            float r = Math.max(e.getAxisValue(android.view.MotionEvent.AXIS_RTRIGGER), e.getAxisValue(android.view.MotionEvent.AXIS_GAS));
            if (l > 0.6f) { finishCapture("lt"); return true; }
            if (r > 0.6f) { finishCapture("rt"); return true; }
            return false;
        }));
        mCapture.show();
    }

    private void finishCapture(String button) {
        if (mCapture == null || mCaptureAction == null)
            return;
        mSettings.setPadButton(mCaptureAction, button);
        mSettings.save(this);
        mCaptureAction = null;
        mCapture.dismiss();
        showMapping();
    }

    private String connectedController() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null || d.isVirtual())
                continue;
            int src = d.getSources();
            if ((src & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD || (src & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK)
                return "Connected: " + d.getName();
        }
        return "None (touch controls)";
    }

    private String socName() {
        return Build.VERSION.SDK_INT >= 31 ? Build.SOC_MODEL : Build.HARDWARE;
    }

    private String ramGb() {
        ActivityManager am = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
        am.getMemoryInfo(mi);
        return String.format(Locale.US, "%.1f GB", mi.totalMem / (1024.0 * 1024 * 1024));
    }

    private static int indexOf(int[] values, int v) {
        for (int i = 0; i < values.length; ++i)
            if (values[i] == v)
                return i;
        return 0;
    }

    private String versionName() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "?";
        }
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_LONG).show();
    }

    // ------------------------------------------------------------------------- cartella del gioco
    private void pickFolder() {
        try {
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), REQ_FOLDER);
        } catch (Exception e) {
            typeFolder();
        }
    }

    private void typeFolder() {
        final EditText e = new EditText(this);
        e.setText(mSettings.gameDir);
        e.setSingleLine(true);
        new AlertDialog.Builder(this, dialogTheme())
            .setTitle("Game folder path")
            .setView(e)
            .setPositiveButton("OK", (d, w) -> { mSettings.gameDir = e.getText().toString().trim(); mSettings.save(this); showSettings(); })
            .setNegativeButton("Cancel", null)
            .show();
    }

    /** "primary:Download/Black Ops" -> /storage/emulated/0/Download/Black Ops */
    private static String treeUriToPath(Uri uri) {
        String id = DocumentsContract.getTreeDocumentId(uri);
        int colon = id.indexOf(':');
        String volume = colon >= 0 ? id.substring(0, colon) : id;
        String rel = colon >= 0 ? id.substring(colon + 1) : "";
        String base = "primary".equalsIgnoreCase(volume) ? Environment.getExternalStorageDirectory().getAbsolutePath() : "/storage/" + volume;
        return rel.isEmpty() ? base : base + "/" + rel;
    }

    // ---------------------------------------------------------------------------- stato e permessi
    private boolean hasStorage() {
        if (Build.VERSION.SDK_INT >= 30)
            return Environment.isExternalStorageManager();
        return checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE) == android.content.pm.PackageManager.PERMISSION_GRANTED;
    }

    private void requestStorage() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName())));
            } catch (Exception e) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else {
            requestPermissions(new String[] { android.Manifest.permission.WRITE_EXTERNAL_STORAGE, android.Manifest.permission.READ_EXTERNAL_STORAGE }, 0);
        }
    }

    private String gameFilesProblem() {
        if (!hasStorage())
            return "Storage access not granted (Settings > Storage access)";
        File game = new File(mSettings.gameDir);
        if (!game.isDirectory())
            return "Game folder not found (Settings > Game folder)";
        if (!new File(game, "zone").isDirectory() || !new File(game, "main").isDirectory())
            return "The game folder needs main/ and zone/ from Black Ops (PC)";
        return null;
    }

    // ------------------------------------------------------------------------------------- avvio
    private void play(String map) {
        play(map, null);
    }

    // cmd: comandi in piu' per il gioco ("set a 1;set b 2"), passati con +set dopo quelli del launcher
    private void play(String map, String cmd) {
        mSettings.save(this);
        if ("v8.2".equals(BuildConfig.CPU_VARIANT) && !cpuHasArmV82()) {
            new AlertDialog.Builder(this, dialogTheme()).setTitle("Wrong version for this device")
                .setMessage("This T5Jool build is optimized for ARMv8.2 CPUs (Snapdragon 845 and newer), but your CPU is older.\n\n"
                    + "Download the \"arm64\" version (without \"v8.2\" in the name) from the releases page.")
                .setPositiveButton("Open releases", (d, w) -> openUrl("https://github.com/" + APP_REPO + "/releases"))
                .setNegativeButton("Cancel", null).show();
            return;
        }
        if (needsDriverSetup()) {
            driverSetup(() -> play(map, cmd));
            return;
        }
        if (!isAdreno() && !updatePrefs().getBoolean("otherGpuNotice", false)) {
            updatePrefs().edit().putBoolean("otherGpuNotice", true).apply();
            new AlertDialog.Builder(this, dialogTheme())
                .setTitle("Experimental GPU")
                .setMessage("T5Jool is tested on Adreno (Snapdragon) GPUs. Your GPU is not tested yet and the game may not start.\n\n"
                    + "Try it with the system driver first. On Mali GPUs (G57, G68, G77, G78, G52, G72, G76) you can also try the "
                    + "PanVK driver: Settings > Download a driver.\n\n"
                    + "Either way, please send us your logs (Settings > Report a problem): they help us add support for your device.")
                .setPositiveButton("Play", (d, w) -> play(map, cmd))
                .setNeutralButton("Get PanVK", (d, w) -> chooseDriverDownload(() -> play(map, cmd)))
                .setNegativeButton("Cancel", null)
                .show();
            return;
        }
        String problem = gameFilesProblem();
        if (problem != null) {
            if (!hasStorage())
                requestStorage();
            else
                new AlertDialog.Builder(this, dialogTheme()).setTitle("Cannot start").setMessage(problem)
                    .setPositiveButton("Settings", (d, w) -> showSettings()).setNegativeButton("Cancel", null).show();
            return;
        }
        T5Settings.root().mkdirs();
        new File(T5Settings.root(), "home").mkdirs();
        T5Settings.logsDir().mkdirs();
        copyAssets("t5jool", new File(mSettings.gameDir));
        copyMenuMedia();
        mCrashNotice = null;
        final boolean mp = mPlayMp;
        mPlayMp = false;
        // T5Jool 0.9: the 64-bit engine loads zones converted once to its layout (<game>/kbz)
        ensureZones(() -> {
            Intent i = new Intent(this, GameActivity.class);
            i.putExtra(GameActivity.EXTRA_MP, mp);
            if (map != null)
                i.putExtra("map", map);
            if (cmd != null)
                i.putExtra("cmd", cmd);
            startActivity(i);
        });
    }

    // ---------------------------------------------------------------------------------- zone conversion
    // Every zone/**/*.ff of the install becomes <game>/kbz/<name>.kbz (libt5convert.so, the converter of
    // tools/ffconv as an executable in the APK), then is compressed with zstd (libt5kbzpack.so).
    // Only missing or older zones are converted, so after the first time this is instant.
    private void findZones(File dir, List<File> out) {
        File[] list = dir.listFiles();
        if (list == null)
            return;
        for (File f : list) {
            if (f.isDirectory())
                findZones(f, out);
            else if (f.getName().toLowerCase(Locale.US).endsWith(".ff"))
                out.add(f);
        }
    }

    private static String zoneName(File ff) {
        String n = ff.getName();
        return n.substring(0, n.length() - 3);
    }

    private void ensureZones(Runnable then) {
        File game = new File(mSettings.gameDir);
        File kbz = new File(game, "kbz");
        List<File> all = new ArrayList<>();
        findZones(new File(game, "zone"), all);
        // each target: the game's zones go to kbz/<name>.kbz, a mod's next to its .ff (mods/<mod>/mod.kbz):
        // the engine looks there, and every mod has its own "mod" zone
        List<File> todo = new ArrayList<>();
        List<File> targets = new ArrayList<>();
        for (File ff : all) {
            File out = new File(kbz, zoneName(ff) + ".kbz");
            if (!out.isFile() || out.lastModified() < ff.lastModified()) {
                todo.add(ff);
                targets.add(out);
            }
        }
        for (String m : T5Settings.mods(mSettings.gameDir)) {
            File[] ffs = new File(new File(game, "mods"), m).listFiles((d, n) -> n.toLowerCase(Locale.US).endsWith(".ff"));
            if (ffs != null)
                for (File ff : ffs) {
                    File out = new File(ff.getParentFile(), zoneName(ff) + ".kbz");
                    if (!out.isFile() || out.lastModified() < ff.lastModified()) {
                        todo.add(ff);
                        targets.add(out);
                    }
                }
        }
        if (todo.isEmpty()) {
            then.run();
            return;
        }
        kbz.mkdirs();
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(24), dp(16), dp(24), dp(8));
        TextView status = text("Preparing the game files (" + todo.size() + " zones). This happens once and takes a few minutes.",
            14, TEXT, false);
        box.addView(status);
        android.widget.ProgressBar bar = new android.widget.ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(todo.size());
        LinearLayout.LayoutParams blp = new LinearLayout.LayoutParams(-1, -2);
        blp.topMargin = dp(16);
        box.addView(bar, blp);
        AlertDialog dlg = new AlertDialog.Builder(this, dialogTheme()).setTitle("First start").setView(box)
            .setCancelable(false).show();
        final String lib = getApplicationInfo().nativeLibraryDir;
        new Thread(() -> {
            int failed = 0;
            StringBuilder log = new StringBuilder();
            for (int n = 0; n < todo.size(); ++n) {
                File ff = todo.get(n);
                final int done = n;
                final String name = zoneName(ff);
                runOnUiThread(() -> {
                    bar.setProgress(done);
                    status.setText("Preparing the game files: " + name + " (" + (done + 1) + "/" + todo.size() + ")");
                });
                File out = targets.get(n);
                File tmp = new File(out.getParentFile(), out.getName() + ".tmp");
                boolean ok = runTool(log, lib + "/libt5convert.so", ff.getAbsolutePath(), tmp.getAbsolutePath())
                    && runTool(log, lib + "/libt5kbzpack.so", tmp.getAbsolutePath(), out.getAbsolutePath());
                tmp.delete();
                if (!ok) {
                    out.delete();
                    ++failed;
                }
            }
            try (OutputStream o = new FileOutputStream(new File(kbz, "convert.log"))) {
                o.write(log.toString().getBytes());
            } catch (Exception ignored) {
            }
            final int bad = failed;
            runOnUiThread(() -> {
                dlg.dismiss();
                if (bad > 0)
                    toast(bad + " zones could not be converted (see kbz/convert.log)");
                then.run();
            });
        }, "T5ZoneConvert").start();
    }

    private static boolean runTool(StringBuilder log, String... cmd) {
        try {
            Process p = new ProcessBuilder(cmd).redirectErrorStream(true).start();
            try (BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()))) {
                String line;
                int lines = 0;
                while ((line = r.readLine()) != null)
                    if (++lines <= 20)
                        log.append(line).append('\n');
            }
            int rc = p.waitFor();
            if (rc != 0)
                log.append("FAILED (").append(rc).append("): ").append(String.join(" ", cmd)).append('\n');
            return rc == 0;
        } catch (Exception e) {
            log.append("FAILED: ").append(e).append('\n');
            return false;
        }
    }

    // mods/ e main/ del port dentro la cartella del gioco (sovrascritti a ogni avvio: piccoli e versionati)
    private void copyAssets(String path, File dest) {
        AssetManager am = getAssets();
        try {
            String[] list = am.list(path);
            if (list == null || list.length == 0) {
                dest.getParentFile().mkdirs();
                try (InputStream in = am.open(path); OutputStream out = new FileOutputStream(dest)) {
                    copy(in, out);
                }
                return;
            }
            dest.mkdirs();
            for (String name : list)
                copyAssets(path + "/" + name, new File(dest, name));
        } catch (Exception e) {
            android.util.Log.w("T5Jool", "copia asset " + path + ": " + e);
        }
    }

    private static void copy(InputStream in, OutputStream out) throws java.io.IOException {
        byte[] buf = new byte[65536];
        int n;
        while ((n = in.read(buf)) > 0)
            out.write(buf, 0, n);
    }

    // ------------------------------------------------------------------------------------- crash e log
    private static String stamp() {
        return new SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(new Date());
    }

    private void checkPreviousCrash() {
        File marker = T5Settings.marker(this);
        if (!marker.exists())
            return;
        marker.delete();
        dumpLogcat("logcat-crash");
        mCrashNotice = "Last session crashed: logs saved. Settings > Report a problem";
        getWindow().getDecorView().postDelayed(() -> {
            if (!isFinishing())
                reportBug(true);
        }, 3600); // dopo l'animazione di avvio
    }

    private File dumpLogcat(String prefix) {
        File dir = T5Settings.logsDir();
        dir.mkdirs();
        File out = new File(dir, prefix + "-" + stamp() + ".txt");
        try {
            java.lang.Process p = Runtime.getRuntime().exec(new String[] { "logcat", "-d", "-v", "threadtime" });
            try (InputStream in = p.getInputStream(); OutputStream os = new FileOutputStream(out)) {
                copy(in, os);
            }
            p.waitFor();
            return out;
        } catch (Exception e) {
            android.util.Log.w("T5Jool", "logcat: " + e);
            return null;
        }
    }

    private void exportLogs() {
        File zip = buildLogsZip();
        if (zip != null)
            new AlertDialog.Builder(this, dialogTheme()).setTitle("Logs exported").setMessage(zip.getAbsolutePath())
                .setPositiveButton("OK", null).setNeutralButton("Report it", (d, w) -> reportBug(false)).show();
    }

    /** Zip dei log recenti in Download; null se non ce ne sono o non si riesce. */
    private File buildLogsZip() {
        dumpLogcat("logcat");
        File dir = T5Settings.logsDir();
        File[] files = dir.listFiles();
        if (files == null || files.length == 0) {
            toast("No logs yet");
            return null;
        }
        File zip = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "T5Jool-logs-" + stamp() + ".zip");
        // solo i log utili: game.txt e game.prev.txt, e gli ultimi 4 logcat e 4 crash (prima finivano nello zip
        // tutti quelli degli ultimi 3 giorni, anche 140 file)
        java.util.Arrays.sort(files, (a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        try (ZipOutputStream zos = new ZipOutputStream(new FileOutputStream(zip))) {
            int logcats = 0, crashes = 0;
            for (File f : files) {
                if (!f.isFile())
                    continue;
                String n = f.getName();
                if (n.startsWith("logcat") && ++logcats > 4)
                    continue;
                if (n.startsWith("crash") && ++crashes > 4)
                    continue;
                zos.putNextEntry(new ZipEntry(f.getName()));
                try (InputStream in = new FileInputStream(f)) {
                    copy(in, zos);
                }
                zos.closeEntry();
            }
            zos.putNextEntry(new ZipEntry("settings.txt"));
            zos.write(("args: " + mSettings.arguments(this) + "\ndevice: " + Build.MANUFACTURER + " " + Build.MODEL
                + " (" + socName() + "), Android " + Build.VERSION.RELEASE + "\n").getBytes());
            zos.closeEntry();
            return zip;
        } catch (Exception e) {
            toast("Export failed: " + e.getMessage());
            return null;
        }
    }

    // ------------------------------------------------------------------------------ segnalazione bug
    static final String ISSUES_URL = "https://github.com/Joolace/T5-Jool---Black-Ops-Recomp/issues/new";

    private String deviceInfo() {
        return "T5Jool " + versionName() + "\n"
            + "Device: " + Build.MANUFACTURER + " " + Build.MODEL + " (" + socName() + ")\n"
            + "Android: " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")\n"
            + "Driver: " + (mSettings.driver == null || mSettings.driver.isEmpty() ? "system" : mSettings.driver) + "\n";
    }

    /**
     * Segnalazione: salva i log in Download e apre una issue GitHub gia' compilata o il Discord (con le informazioni
     * del dispositivo copiate negli appunti). Lo zip va allegato a mano: Android non permette di allegarlo da qui.
     */
    private void reportBug(boolean afterCrash) {
        File zip = buildLogsZip();
        String where = zip != null ? "Download/" + zip.getName() : "(no logs found)";
        String msg = (afterCrash ? "T5Jool closed unexpectedly last time. " : "")
            + "Your logs were saved to:\n" + where + "\n\n"
            + "Open a GitHub issue or post on Discord, describe what happened and attach that zip file.";
        new AlertDialog.Builder(this, dialogTheme())
            .setTitle(afterCrash ? "Send a crash report?" : "Report a problem")
            .setMessage(msg)
            .setPositiveButton("GitHub issue", (d, w) -> openIssue(where, afterCrash))
            .setNeutralButton("Discord", (d, w) -> {
                copyToClipboard(deviceInfo() + "Logs: " + where);
                toast("Device info copied: paste it in your message and attach the zip");
                openDiscord();
            })
            .setNegativeButton(afterCrash ? "Not now" : "Cancel", null)
            .show();
    }

    private void openIssue(String logs, boolean crash) {
        String body = "**What happened?**\n\n\n**Steps to reproduce**\n1. \n\n**Map / mode**\n\n\n"
            + "**System**\n```\n" + deviceInfo() + "```\n\n"
            + "**Logs**\nPlease drag and drop `" + logs + "` here.\n";
        String title = crash ? "Crash: " : "Bug: ";
        try {
            Uri u = Uri.parse(ISSUES_URL).buildUpon()
                .appendQueryParameter("title", title)
                .appendQueryParameter("body", body)
                .appendQueryParameter("labels", crash ? "crash" : "bug")
                .build();
            startActivity(new Intent(Intent.ACTION_VIEW, u));
        } catch (Exception e) {
            copyToClipboard(body);
            toast("Report copied: open " + ISSUES_URL);
        }
    }

    private void copyToClipboard(String text) {
        try {
            ClipboardManager cm = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
            if (cm != null)
                cm.setPrimaryClip(ClipData.newPlainText("T5Jool report", text));
        } catch (Exception ignored) {
        }
    }

    // --------------------------------------------------------------------------------- driver Vulkan
    private List<String> driverNames() {
        List<String> names = new ArrayList<>();
        File[] dirs = T5Settings.driversDir(this).listFiles();
        if (dirs != null)
            for (File d : dirs)
                if (d.isDirectory())
                    names.add(d.getName());
        java.util.Collections.sort(names);
        return names;
    }

    private String[] driverLabels() {
        List<String> labels = new ArrayList<>();
        labels.add("System (Qualcomm)");
        labels.addAll(driverNames());
        return labels.toArray(new String[0]);
    }

    private int driverIndex() {
        int idx = driverNames().indexOf(mSettings.driver);
        return idx >= 0 ? idx + 1 : 0;
    }

    private void importDriver() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, REQ_DRIVER);
    }

    private void deleteDriver() {
        if (mSettings.driver.isEmpty())
            return;
        deleteTree(new File(T5Settings.driversDir(this), mSettings.driver));
        mSettings.driver = "";
        mSettings.save(this);
        showSettings();
    }

    private static void deleteTree(File f) {
        File[] children = f.listFiles();
        if (children != null)
            for (File c : children)
                deleteTree(c);
        f.delete();
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (result != RESULT_OK || data == null || data.getData() == null)
            return;
        if (request == REQ_LSFG_DLL) {
            // Lossless.dll dell'utente: copiata nella memoria privata dell'app, non esce mai da qui
            File dst = T5Settings.lsfgDll(this);
            dst.getParentFile().mkdirs();
            try (InputStream in = getContentResolver().openInputStream(data.getData());
                 OutputStream os = new FileOutputStream(dst)) {
                copy(in, os);
                if (dst.length() < 100000) {
                    dst.delete();
                    toast("That is not Lossless.dll");
                } else
                    toast("Lossless.dll imported");
            } catch (Exception e) {
                dst.delete();
                toast("Import failed: " + e.getMessage());
            }
            showSettings();
            return;
        }
        if (request == REQ_FOLDER) {
            mSettings.gameDir = treeUriToPath(data.getData());
            mSettings.save(this);
            showSettings();
            return;
        }
        if (request != REQ_DRIVER)
            return;
        try {
            installDriverZip(getContentResolver().openInputStream(data.getData()));
        } catch (Exception e) {
            toast("Import failed: " + e.getMessage());
        }
    }

    /** Installa uno zip di driver (libreria .so + meta.json) e lo seleziona; true se riuscito. */
    private boolean installDriverZip(InputStream in) {
        String name = "driver-" + stamp();
        File dir = new File(T5Settings.driversDir(this), name);
        dir.mkdirs();
        boolean hasLib = false;
        String metaName = null;
        try (ZipInputStream zis = new ZipInputStream(in)) {
            ZipEntry e;
            while ((e = zis.getNextEntry()) != null) {
                if (e.isDirectory())
                    continue;
                String file = new File(e.getName()).getName(); // niente sottocartelle
                File out = new File(dir, file);
                try (OutputStream os = new FileOutputStream(out)) {
                    copy(zis, os);
                }
                if (file.endsWith(".so"))
                    hasLib = true;
                if (file.equals("meta.json"))
                    metaName = readMetaName(out);
            }
        } catch (Exception e) {
            toast("Import failed: " + e.getMessage());
            deleteTree(dir);
            return false;
        }
        if (!hasLib) {
            toast("No .so driver library in that zip");
            deleteTree(dir);
            return false;
        }
        if (metaName != null) {
            String safe = metaName.replaceAll("[^A-Za-z0-9._ -]", "_").trim();
            File named = new File(T5Settings.driversDir(this), safe);
            if (!safe.isEmpty() && !named.exists() && dir.renameTo(named))
                name = safe;
        }
        mSettings.driver = name;
        mSettings.save(this);
        toast("Driver installed: " + name);
        if (mInSettings)
            showSettings();
        else
            showHome();
        return true;
    }

    // ------------------------------------------------------------------------------ aggiornamenti
    // All'avvio (al massimo ogni 12 ore) e da Settings > Check for updates: nuova versione di T5Jool sulle release
    // GitHub (anche le pre-release) e nuova versione del driver Turnip scaricato dal lanciatore (stessa fonte e stessa
    // variante: il nome del file senza numeri).
    static final String APP_REPO = "Joolace/T5-Jool---Black-Ops-Recomp";
    private static final long UPDATE_INTERVAL = 12L * 3600 * 1000;

    private SharedPreferences updatePrefs() {
        return getSharedPreferences("t5jool_updates", MODE_PRIVATE);
    }

    private static String httpGet(String url) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setRequestProperty("Accept", "application/vnd.github+json");
        c.setRequestProperty("User-Agent", "T5Jool-launcher");
        c.setConnectTimeout(10000);
        c.setReadTimeout(15000);
        if (c.getResponseCode() != 200)
            throw new Exception("GitHub " + c.getResponseCode());
        StringBuilder sb = new StringBuilder();
        try (BufferedReader r = new BufferedReader(new InputStreamReader(c.getInputStream()))) {
            String line;
            while ((line = r.readLine()) != null)
                sb.append(line).append('\n');
        }
        return sb.toString();
    }

    /** confronto di versioni "0.1-alpha", "0.2-beta2", "1.0": numeri, poi alpha < beta < rc < finale */
    static int compareVersions(String a, String b) {
        a = a.replaceFirst("^[vV]", "").toLowerCase(Locale.US);
        b = b.replaceFirst("^[vV]", "").toLowerCase(Locale.US);
        String[] pa = a.split("-", 2), pb = b.split("-", 2);
        String[] na = pa[0].split("\\."), nb = pb[0].split("\\.");
        for (int i = 0; i < Math.max(na.length, nb.length); ++i) {
            int x = i < na.length ? parseIntSafe(na[i]) : 0, y = i < nb.length ? parseIntSafe(nb[i]) : 0;
            if (x != y)
                return Integer.compare(x, y);
        }
        return Integer.compare(suffixRank(pa.length > 1 ? pa[1] : ""), suffixRank(pb.length > 1 ? pb[1] : ""));
    }

    private static int parseIntSafe(String s) {
        try {
            return Integer.parseInt(s.replaceAll("[^0-9]", ""));
        } catch (Exception e) {
            return 0;
        }
    }

    private static int suffixRank(String s) {
        int n = parseIntSafe(s); // alpha2 -> 2
        if (s.isEmpty())
            return 3000;
        if (s.startsWith("rc"))
            return 2000 + n;
        if (s.startsWith("beta"))
            return 1000 + n;
        return n; // alpha
    }

    /** la CPU ha le istruzioni della variante "v8.2" (-march=armv8.2-a+fp16+dotprod+rcpc): atomics, asimdhp, asimddp, lrcpc */
    static boolean cpuHasArmV82() {
        try (BufferedReader r = new BufferedReader(new FileReader("/proc/cpuinfo"))) {
            String line;
            while ((line = r.readLine()) != null)
                if (line.startsWith("Features") && line.contains(" atomics") && line.contains(" asimddp")
                    && line.contains(" asimdhp") && line.contains(" lrcpc"))
                    return true;
        } catch (Exception e) {
            return true; // se non si legge, non si blocca nessuno
        }
        return false;
    }

    private static String driverFamily(String assetName) {
        return assetName.toLowerCase(Locale.US).replaceAll("[0-9]", "").replaceAll("[^a-z]", "");
    }

    private void checkUpdates(boolean manual) {
        SharedPreferences up = updatePrefs();
        long now = System.currentTimeMillis();
        if (!manual && now - up.getLong("lastCheck", 0) < UPDATE_INTERVAL)
            return;
        if (manual)
            toast("Checking for updates...");
        new Thread(() -> {
            JSONObject appRelease = null;
            DriverAsset newDriver = null;
            String error = null;
            try {
                JSONArray rels = new JSONArray(httpGet("https://api.github.com/repos/" + APP_REPO + "/releases?per_page=5"));
                for (int i = 0; i < rels.length(); ++i) {
                    JSONObject r = rels.getJSONObject(i);
                    if (r.optBoolean("draft"))
                        continue;
                    if (compareVersions(r.optString("tag_name", "0"), versionName()) > 0
                        && (appRelease == null || compareVersions(r.optString("tag_name"), appRelease.optString("tag_name")) > 0))
                        appRelease = r;
                }
            } catch (Exception e) {
                error = e.getMessage();
            }
            // driver: solo se e' ancora selezionato quello scaricato dal lanciatore
            String repo = up.getString("drvRepo", null), name = up.getString("drvName", ""), pub = up.getString("drvPublished", "");
            if (repo != null && up.getString("drvDir", "").equals(mSettings.driver)) {
                String filter = "";
                for (String[] src : DRIVER_SOURCES)
                    if (src[0].equals(repo))
                        filter = src[1];
                for (String[] src : DRIVER_SOURCES_MALI)
                    if (src[0].equals(repo))
                        filter = src[1];
                try {
                    for (DriverAsset a : fetchDriverAssets(repo, filter))
                        if (driverFamily(a.name).equals(driverFamily(name)) && a.published.compareTo(pub) > 0
                            && (newDriver == null || a.published.compareTo(newDriver.published) > 0))
                            newDriver = a;
                } catch (Exception e) {
                    if (error == null)
                        error = e.getMessage();
                }
            }
            up.edit().putLong("lastCheck", now).apply();
            final JSONObject rel = appRelease;
            final DriverAsset drv = newDriver;
            final String err = error;
            runOnUiThread(() -> {
                if (isFinishing())
                    return;
                if (rel != null)
                    offerAppUpdate(rel, drv);
                else if (drv != null)
                    offerDriverUpdate(drv);
                else if (manual)
                    toast(err != null ? "Could not check for updates: " + err : "You're up to date: T5Jool " + versionName());
            });
        }).start();
    }

    private void offerAppUpdate(JSONObject rel, DriverAsset drv) {
        String tag = rel.optString("tag_name");
        String notes = rel.optString("body", "").replace("\r", "").trim();
        if (notes.length() > 900)
            notes = notes.substring(0, 900) + "...";
        // due APK per release: "-arm64-v8.2.apk" (ARMv8.2-A) e "-arm64.apk" (generico): si prende quello della stessa
        // variante di questa installazione; se c'e' un solo APK, quello.
        String apkUrl = null, anyUrl = null;
        long apkSize = 0, anySize = 0;
        boolean wantV82 = "v8.2".equals(BuildConfig.CPU_VARIANT) && cpuHasArmV82();
        JSONArray assets = rel.optJSONArray("assets");
        if (assets != null)
            for (int i = 0; i < assets.length(); ++i) {
                JSONObject a = assets.optJSONObject(i);
                String n = a != null ? a.optString("name").toLowerCase(Locale.US) : "";
                if (!n.endsWith(".apk"))
                    continue;
                anyUrl = a.optString("browser_download_url");
                anySize = a.optLong("size");
                if (n.contains("v8.2") == wantV82) {
                    apkUrl = anyUrl;
                    apkSize = anySize;
                }
            }
        if (apkUrl == null) {
            apkUrl = anyUrl;
            apkSize = anySize;
        }
        final String url = apkUrl;
        final long size = apkSize;
        final String page = rel.optString("html_url", "https://github.com/" + APP_REPO + "/releases");
        AlertDialog.Builder b = new AlertDialog.Builder(this, dialogTheme())
            .setTitle("T5Jool " + tag + " is available")
            .setMessage("You have " + versionName() + ".\n\n" + (notes.isEmpty() ? "" : notes))
            .setPositiveButton(url != null ? "Update" : "Open page", (d, w) -> {
                if (url != null)
                    downloadAppUpdate(url, size, tag, page);
                else
                    openUrl(page);
            })
            .setNegativeButton("Later", (d, w) -> {
                if (drv != null)
                    offerDriverUpdate(drv);
            });
        b.show();
    }

    private void offerDriverUpdate(DriverAsset drv) {
        new AlertDialog.Builder(this, dialogTheme())
            .setTitle("New Turnip driver")
            .setMessage("A newer version of your GPU driver is available:\n\n" + drv.name.replace(".zip", "")
                + "\n" + drv.published.substring(0, Math.min(10, drv.published.length()))
                + "\n\nYour current driver stays installed: you can switch back in Settings.")
            .setPositiveButton("Update", (d, w) -> downloadDriver(drv, null))
            .setNegativeButton("Later", null)
            .show();
    }

    private void openUrl(String url) {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
        } catch (Exception e) {
            toast(url);
        }
    }

    private void downloadAppUpdate(String url, long size, String tag, String page) {
        // Android 8+: serve il permesso "installa app sconosciute" per questa app
        if (Build.VERSION.SDK_INT >= 26 && !getPackageManager().canRequestPackageInstalls()) {
            toast("Allow T5Jool to install updates, then press Update again");
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + getPackageName())));
            } catch (Exception e) {
                openUrl(page);
            }
            return;
        }
        android.widget.ProgressBar bar = new android.widget.ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(100);
        bar.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));
        FrameLayout box = new FrameLayout(this);
        box.setPadding(dp(24), dp(12), dp(24), dp(4));
        box.addView(bar);
        AlertDialog dlg = new AlertDialog.Builder(this, dialogTheme()).setTitle("Downloading T5Jool " + tag)
            .setView(box).setCancelable(false).show();
        new Thread(() -> {
            String error = null;
            try {
                PackageInstaller pi = getPackageManager().getPackageInstaller();
                PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
                params.setAppPackageName(getPackageName());
                int id = pi.createSession(params);
                try (PackageInstaller.Session session = pi.openSession(id)) {
                    HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
                    c.setRequestProperty("User-Agent", "T5Jool-launcher");
                    c.setConnectTimeout(10000);
                    c.setReadTimeout(30000);
                    long total = c.getContentLengthLong() > 0 ? c.getContentLengthLong() : size;
                    try (InputStream in = c.getInputStream(); OutputStream os = session.openWrite("t5jool.apk", 0, total > 0 ? total : -1)) {
                        byte[] buf = new byte[65536];
                        long done = 0;
                        int n, last = -1;
                        while ((n = in.read(buf)) > 0) {
                            os.write(buf, 0, n);
                            done += n;
                            int pct = total > 0 ? (int) (done * 100 / total) : 0;
                            if (pct != last) {
                                last = pct;
                                final int p = pct;
                                runOnUiThread(() -> bar.setProgress(p));
                            }
                        }
                        session.fsync(os);
                    }
                    Intent cb = new Intent(ACTION_INSTALL_STATUS).setPackage(getPackageName());
                    int flags = Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0;
                    PendingIntent sender = PendingIntent.getBroadcast(this, id, cb, flags | PendingIntent.FLAG_UPDATE_CURRENT);
                    runOnUiThread(this::registerInstallReceiver);
                    session.commit(sender.getIntentSender());
                }
            } catch (Exception e) {
                error = e.getMessage();
            }
            final String err = error;
            runOnUiThread(() -> {
                dlg.dismiss();
                if (err != null) {
                    toast("Update failed: " + err);
                    openUrl(page);
                }
            });
        }).start();
    }

    private static final String ACTION_INSTALL_STATUS = "com.joolace.t5jool.INSTALL_STATUS";
    private BroadcastReceiver mInstallReceiver;

    private void registerInstallReceiver() {
        if (mInstallReceiver != null)
            return;
        mInstallReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
                if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
                    Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
                    if (confirm != null) {
                        confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        startActivity(confirm); // conferma di Android "Aggiornare questa app?"
                    }
                } else if (status != PackageInstaller.STATUS_SUCCESS) {
                    String msg = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
                    toast("Update not installed" + (msg != null ? ": " + msg : ""));
                }
            }
        };
        IntentFilter f = new IntentFilter(ACTION_INSTALL_STATUS);
        if (Build.VERSION.SDK_INT >= 33)
            registerReceiver(mInstallReceiver, f, Context.RECEIVER_NOT_EXPORTED);
        else
            registerReceiver(mInstallReceiver, f);
    }

    // ---------------------------------------------------------------- driver: download e installazione guidata
    // Driver Turnip provati con T5Jool (release GitHub): tutti quelli di purple-turnip, solo i Turnip di StevenMXZ.
    private static final String[][] DRIVER_SOURCES = {
        { "MrPurple666/purple-turnip", "" },
        { "StevenMXZ/Adreno-Tools-Drivers", "turnip" },
    };
    // GPU Mali (sperimentale, non provato): PanVK di FristOneRR (Valhall G57/G68/G77/G78, Bifrost G52/G71/G72/G76)
    private static final String[][] DRIVER_SOURCES_MALI = {
        { "FristOneRR/FristOneRR-Panvk-Driver", "panvk" },
    };

    private String[][] driverSources() {
        return isAdreno() ? DRIVER_SOURCES : DRIVER_SOURCES_MALI;
    }

    private static final class DriverAsset {
        String repo, release, name, url;
        long size;
        String published;
    }

    private boolean isAdreno() {
        String soc = (socName() + " " + Build.HARDWARE + " " + Build.BOARD).toLowerCase(Locale.US);
        if (Build.VERSION.SDK_INT >= 31 && Build.SOC_MANUFACTURER != null
            && Build.SOC_MANUFACTURER.toLowerCase(Locale.US).contains("qti"))
            return true;
        return soc.contains("qcom") || soc.contains("sm8") || soc.contains("sm7") || soc.contains("sm6")
            || soc.contains("cq8") || soc.contains("kona") || soc.contains("lahaina") || soc.contains("taro")
            || soc.contains("kalama") || soc.contains("pineapple") || soc.contains("sun");
    }

    /** Prima di giocare: su Adreno con il driver di sistema (che non funziona) si propone l'installazione guidata. */
    private boolean needsDriverSetup() {
        return isAdreno() && (mSettings.driver == null || mSettings.driver.isEmpty())
            && !updatePrefs().getBoolean("trySystemDriver", false);
    }

    private void driverSetup(Runnable afterInstall) {
        new AlertDialog.Builder(this, dialogTheme())
            .setTitle("Install a Turnip driver?")
            .setMessage("On Adreno GPUs T5Jool runs best with a Turnip Vulkan driver: more FPS and no stutter while shaders compile.\n\n"
                + "The stock Qualcomm driver also works, but it is slower and stutters for the first games (it gets better as "
                + "the shader cache fills).\n\nDownload Turnip now? It takes a few seconds (about 10-20 MB).")
            .setPositiveButton("Download", (d, w) -> chooseDriverDownload(afterInstall))
            .setNeutralButton("Import .zip", (d, w) -> importDriver())
            .setNegativeButton("Try system driver", (d, w) -> {
                // T5Jool 0.5: prova con il driver di Qualcomm (per capire cosa manca; la scelta resta)
                updatePrefs().edit().putBoolean("trySystemDriver", true).apply();
                toast("Using the system driver. If the game closes, send the logs (Settings > Help and about)");
                if (afterInstall != null)
                    afterInstall.run();
            })
            .show();
    }

    private void chooseDriverDownload(Runnable afterInstall) {
        AlertDialog wait = new AlertDialog.Builder(this, dialogTheme()).setTitle("Looking for drivers...")
            .setMessage("Checking the latest Turnip releases on GitHub").setCancelable(true).show();
        new Thread(() -> {
            List<DriverAsset> list = new ArrayList<>();
            String error = null;
            // per ogni fonte le 6 versioni piu' recenti, una fonte dopo l'altra (prima StevenMXZ, con molti file per
            // release, riempiva tutta la lista e quelli di Mr Purple non comparivano)
            for (String[] src : driverSources()) {
                try {
                    List<DriverAsset> part = fetchDriverAssets(src[0], src[1]);
                    java.util.Collections.sort(part, (a, b) -> b.published.compareTo(a.published));
                    list.addAll(part.subList(0, Math.min(6, part.size())));
                } catch (Exception e) {
                    error = src[0] + ": " + e.getMessage();
                }
            }
            final String err = error;
            runOnUiThread(() -> {
                wait.dismiss();
                if (list.isEmpty()) {
                    new AlertDialog.Builder(this, dialogTheme()).setTitle("No drivers found")
                        .setMessage("Could not reach GitHub" + (err != null ? " (" + err + ")" : "") + ". Check your connection, or download a Turnip .zip in the browser and use Import .zip.")
                        .setPositiveButton("OK", null).show();
                    return;
                }
                int n = list.size();
                String[] labels = new String[n];
                String prevRepo = "";
                for (int i = 0; i < n; ++i) {
                    DriverAsset a = list.get(i);
                    String who = a.repo.startsWith("MrPurple") ? "Mr Purple (purple-turnip)"
                        : a.repo.startsWith("FristOneRR") ? "FristOneRR (PanVK, experimental)" : "StevenMXZ";
                    boolean first = !a.repo.equals(prevRepo);
                    prevRepo = a.repo;
                    labels[i] = (first ? "[" + who + "]\n" : "") + a.name.replace(".zip", "")
                        + "\n" + a.published.substring(0, Math.min(10, a.published.length()))
                        + String.format(Locale.US, " - %.1f MB", a.size / 1048576.0) + (first ? "  (newest)" : "");
                }
                if (err != null)
                    toast("Some drivers could not be listed: " + err);
                new AlertDialog.Builder(this, dialogTheme()).setTitle("Choose a Turnip driver")
                    .setItems(labels, (d, which) -> downloadDriver(list.get(which), afterInstall))
                    .setNegativeButton("Cancel", null).show();
            });
        }).start();
    }

    private static List<DriverAsset> fetchDriverAssets(String repo, String mustContain) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL("https://api.github.com/repos/" + repo + "/releases?per_page=8").openConnection();
        c.setRequestProperty("Accept", "application/vnd.github+json");
        c.setRequestProperty("User-Agent", "T5Jool-launcher");
        c.setConnectTimeout(10000);
        c.setReadTimeout(15000);
        if (c.getResponseCode() != 200)
            throw new Exception("GitHub " + c.getResponseCode());
        StringBuilder sb = new StringBuilder();
        try (BufferedReader r = new BufferedReader(new InputStreamReader(c.getInputStream()))) {
            String line;
            while ((line = r.readLine()) != null)
                sb.append(line);
        }
        List<DriverAsset> out = new ArrayList<>();
        JSONArray releases = new JSONArray(sb.toString());
        for (int i = 0; i < releases.length(); ++i) {
            JSONObject rel = releases.getJSONObject(i);
            JSONArray assets = rel.optJSONArray("assets");
            if (assets == null)
                continue;
            for (int j = 0; j < assets.length(); ++j) {
                JSONObject as = assets.getJSONObject(j);
                String name = as.optString("name", "");
                String lower = name.toLowerCase(Locale.US);
                if (!(lower.endsWith(".zip") || lower.endsWith(".adpkg")) || (!mustContain.isEmpty() && !lower.contains(mustContain)))
                    continue;
                DriverAsset a = new DriverAsset();
                a.repo = repo;
                a.release = rel.optString("name", rel.optString("tag_name", ""));
                a.name = name;
                a.url = as.optString("browser_download_url", "");
                a.size = as.optLong("size", 0);
                a.published = rel.optString("published_at", "");
                if (!a.url.isEmpty())
                    out.add(a);
            }
        }
        return out;
    }

    private void downloadDriver(DriverAsset a, Runnable afterInstall) {
        android.widget.ProgressBar bar = new android.widget.ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(100);
        bar.setProgressTintList(android.content.res.ColorStateList.valueOf(ACCENT));
        FrameLayout box = new FrameLayout(this);
        box.setPadding(dp(24), dp(12), dp(24), dp(4));
        box.addView(bar);
        AlertDialog dlg = new AlertDialog.Builder(this, dialogTheme()).setTitle("Downloading " + a.name)
            .setView(box).setCancelable(false).show();
        new Thread(() -> {
            File tmp = new File(getCacheDir(), "driver-download.zip");
            String error = null;
            try {
                HttpURLConnection c = (HttpURLConnection) new URL(a.url).openConnection();
                c.setRequestProperty("User-Agent", "T5Jool-launcher");
                c.setConnectTimeout(10000);
                c.setReadTimeout(30000);
                c.setInstanceFollowRedirects(true);
                long total = c.getContentLengthLong() > 0 ? c.getContentLengthLong() : a.size;
                try (InputStream in = c.getInputStream(); OutputStream os = new FileOutputStream(tmp)) {
                    byte[] buf = new byte[65536];
                    long done = 0;
                    int n, last = -1;
                    while ((n = in.read(buf)) > 0) {
                        os.write(buf, 0, n);
                        done += n;
                        int pct = total > 0 ? (int) (done * 100 / total) : 0;
                        if (pct != last) {
                            last = pct;
                            final int p = pct;
                            runOnUiThread(() -> bar.setProgress(p));
                        }
                    }
                }
            } catch (Exception e) {
                error = e.getMessage();
            }
            final String err = error;
            runOnUiThread(() -> {
                dlg.dismiss();
                if (err != null) {
                    toast("Download failed: " + err);
                    return;
                }
                boolean ok;
                try {
                    ok = installDriverZip(new FileInputStream(tmp));
                    if (ok)
                        updatePrefs().edit().putString("drvRepo", a.repo).putString("drvName", a.name)
                            .putString("drvPublished", a.published).putString("drvDir", mSettings.driver).apply();
                } catch (Exception e) {
                    ok = false;
                    toast("Install failed: " + e.getMessage());
                }
                tmp.delete();
                if (ok && afterInstall != null)
                    afterInstall.run();
            });
        }).start();
    }

    private static String readMetaName(File meta) {
        try (BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(meta)))) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = r.readLine()) != null)
                sb.append(line);
            String s = sb.toString();
            int k = s.indexOf("\"name\"");
            if (k < 0)
                return null;
            int a = s.indexOf('"', s.indexOf(':', k) + 1);
            int b = a >= 0 ? s.indexOf('"', a + 1) : -1;
            return a >= 0 && b > a ? s.substring(a + 1, b) : null;
        } catch (Exception e) {
            return null;
        }
    }
}

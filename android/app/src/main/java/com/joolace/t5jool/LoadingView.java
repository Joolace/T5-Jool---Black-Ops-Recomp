package com.joolace.t5jool;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.LinearInterpolator;

/**
 * Schermata di caricamento: copre il nero mentre il gioco riparte in un processo nuovo (nuova mappa, salvataggio
 * caricato: memoria pulita) e mentre il motore si avvia. Logo T5 che pulsa, un arco che gira, il nome della mappa e
 * una riga che spiega cosa succede. Sparisce con una dissolvenza quando il gioco disegna i primi frame (hide()).
 */
public class LoadingView extends View {
    private static final int ACCENT = Color.rgb(222, 52, 40);

    private final Bitmap mLogo;
    private final String mTitle;
    private final String mSubtitle;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint mText = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ValueAnimator mAnim;
    private final long mStart = System.currentTimeMillis();
    private long mFadeStart = 0;

    public LoadingView(Context ctx, String title, String subtitle) {
        super(ctx);
        mLogo = BitmapFactory.decodeResource(ctx.getResources(), R.drawable.logo_t5);
        mTitle = title;
        mSubtitle = subtitle;
        setBackgroundColor(Color.TRANSPARENT);
        mAnim = ValueAnimator.ofFloat(0f, 1f);
        mAnim.setDuration(1000);
        mAnim.setRepeatCount(ValueAnimator.INFINITE);
        mAnim.setInterpolator(new LinearInterpolator());
        mAnim.addUpdateListener(a -> invalidate());
    }

    /** Sopra tutto il contenuto dell'attivita'. */
    public static LoadingView show(android.app.Activity a, String cmd, boolean relaunch) {
        LoadingView v = new LoadingView(a, titleFor(cmd), subtitleFor(cmd, relaunch));
        ViewGroup root = (ViewGroup) a.getWindow().getDecorView();
        root.addView(v, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        v.mAnim.start();
        return v;
    }

    /** Dissolvenza e via (si puo' chiamare da qualunque thread). */
    public void hide() {
        post(() -> {
            if (mFadeStart == 0)
                mFadeStart = System.currentTimeMillis();
        });
    }

    private void remove() {
        mAnim.cancel();
        ViewGroup parent = (ViewGroup) getParent();
        if (parent != null)
            parent.removeView(this);
    }

    // i tocchi passano al gioco solo dopo che la schermata e' sparita
    @Override
    public boolean onTouchEvent(MotionEvent e) {
        return mFadeStart == 0;
    }

    /** "set systemlink 0;set t5save_continue 1;map zombie_theater" -> "Kino der Toten" */
    static String titleFor(String cmd) {
        String map = argAfter(cmd, "map");
        if (map == null)
            map = argAfter(cmd, "devmap");
        if (map != null)
            return mapTitle(map);
        if (cmd != null && cmd.contains("connect "))
            return "Joining the game";
        return "Black Ops Recomp";
    }

    static String subtitleFor(String cmd, boolean relaunch) {
        if (cmd != null && cmd.contains("t5save_continue 1"))
            return "Loading your saved game";
        if (cmd != null && cmd.contains("connect "))
            return relaunch ? "Restarting with free memory, then connecting" : "Connecting";
        if (relaunch)
            return "Restarting with free memory for the next map";
        return "Starting";
    }

    private static String argAfter(String cmd, String word) {
        if (cmd == null)
            return null;
        for (String part : cmd.split("[;\\n]")) {
            String[] t = part.trim().split("\\s+");
            if (t.length >= 2 && t[0].equals(word))
                return t[1];
        }
        return null;
    }

    static String mapTitle(String map) {
        switch (map) {
            case "zombie_theater": return "Kino der Toten";
            case "zombie_pentagon": return "\"Five\"";
            case "zombie_cosmodrome": return "Ascension";
            case "zombie_coast": return "Call of the Dead";
            case "zombie_temple": return "Shangri-La";
            case "zombie_moon": return "Moon";
            case "zombie_cod5_prototype": return "Nacht der Untoten";
            case "zombie_cod5_asylum": return "Verruckt";
            case "zombie_cod5_sumpf": return "Shi No Numa";
            case "zombie_cod5_factory": return "Der Riese";
            case "zombietron": return "Dead Ops Arcade";
            default: return map;
        }
    }

    @Override
    protected void onDraw(Canvas c) {
        final long now = System.currentTimeMillis();
        final float t = (now - mStart) / 1000f;
        float alpha = Math.min(1f, (now - mStart) / 250f); // entra in fretta: copre subito il nero
        if (mFadeStart != 0) {
            float f = (now - mFadeStart) / 400f;
            if (f >= 1f) {
                post(this::remove);
                return;
            }
            alpha *= 1f - f;
        }
        final int w = getWidth(), h = getHeight();
        final float u = Math.min(w, h) / 720f;
        final int a = (int) (255 * alpha);

        mPaint.setShader(null);
        mPaint.setColor(Color.argb(a, 8, 8, 10));
        c.drawRect(0, 0, w, h, mPaint);
        float pulse = 0.8f + 0.2f * (float) Math.sin(t * 2.6);
        mPaint.setShader(new RadialGradient(w / 2f, h * 0.42f, Math.max(w, h) * 0.5f,
            Color.argb((int) (80 * pulse * alpha), 150, 20, 16), Color.TRANSPARENT, Shader.TileMode.CLAMP));
        c.drawRect(0, 0, w, h, mPaint);
        mPaint.setShader(null);

        // logo che respira, dentro un arco che gira
        final float cx = w / 2f, cy = h * 0.42f;
        final float r = 110 * u;
        if (mLogo != null) {
            float s = (r * 1.25f) / Math.max(mLogo.getWidth(), mLogo.getHeight()) * (0.96f + 0.04f * pulse);
            float lw = mLogo.getWidth() * s, lh = mLogo.getHeight() * s;
            mPaint.setAlpha(a);
            c.drawBitmap(mLogo, null, new RectF(cx - lw / 2, cy - lh / 2, cx + lw / 2, cy + lh / 2), mPaint);
            mPaint.setAlpha(255);
        }
        Paint arc = new Paint(Paint.ANTI_ALIAS_FLAG);
        arc.setStyle(Paint.Style.STROKE);
        arc.setStrokeCap(Paint.Cap.ROUND);
        arc.setStrokeWidth(6 * u);
        arc.setColor(Color.argb((int) (60 * alpha), 255, 255, 255));
        RectF ring = new RectF(cx - r, cy - r, cx + r, cy + r);
        c.drawOval(ring, arc);
        arc.setColor(Color.argb(a, Color.red(ACCENT), Color.green(ACCENT), Color.blue(ACCENT)));
        float start = (t * 300f) % 360f;
        float sweep = 70f + 50f * (float) Math.sin(t * 3.1);
        c.drawArc(ring, start, sweep, false, arc);

        // testi
        mText.setTextAlign(Paint.Align.CENTER);
        mText.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
        mText.setTextSize(40 * u);
        mText.setColor(Color.argb(a, 240, 240, 240));
        c.drawText(mTitle, cx, cy + r + 80 * u, mText);
        mText.setTypeface(Typeface.DEFAULT);
        mText.setTextSize(21 * u);
        mText.setColor(Color.argb((int) (a * 0.7f), 220, 220, 220));
        int dots = (int) (t * 2.5f) % 4;
        c.drawText(mSubtitle + "...".substring(0, dots) + "   ".substring(0, 3 - dots), cx, cy + r + 118 * u, mText);
    }
}

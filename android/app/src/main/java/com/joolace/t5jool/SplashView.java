package com.joolace.t5jool;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.LinearInterpolator;

/**
 * Schermata di avvio del lanciatore: tre graffi rossi che tagliano lo schermo, il logo T5 che entra con un piccolo
 * rimbalzo e un bagliore, il titolo lettera per lettera, versione e sviluppatore, poi dissolvenza. Un tocco o un tasto
 * la salta. Tutto disegnato qui (nessuna risorsa oltre a logo_t5).
 */
public class SplashView extends View {
    private static final long DURATION = 3200;
    private static final int ACCENT = Color.rgb(222, 52, 40);

    private final Bitmap mLogo;
    private final String mVersion;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
    private final Paint mGlow = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mText = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ValueAnimator mAnim;
    private Runnable mOnDone;
    private boolean mDone;
    private float mT; // millisecondi dall'inizio

    public SplashView(Context ctx) {
        super(ctx);
        setFocusable(true);
        setFocusableInTouchMode(true);
        mLogo = BitmapFactory.decodeResource(ctx.getResources(), R.drawable.logo_t5);
        String v = "";
        try {
            PackageInfo pi = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0);
            v = pi.versionName;
        } catch (Exception ignored) {
        }
        mVersion = v;
        setLayerType(LAYER_TYPE_SOFTWARE, null); // BlurMaskFilter del bagliore
        mAnim = ValueAnimator.ofFloat(0f, DURATION);
        mAnim.setDuration(DURATION);
        mAnim.setInterpolator(new LinearInterpolator());
        mAnim.addUpdateListener(a -> {
            mT = (float) a.getAnimatedValue();
            invalidate();
            if (mT >= DURATION)
                finish();
        });
    }

    /** Aggiunge la schermata sopra tutto e la avvia; onDone quando sparisce. */
    public static void show(android.app.Activity a, Runnable onDone) {
        SplashView v = new SplashView(a);
        v.mOnDone = onDone;
        ViewGroup root = (ViewGroup) a.getWindow().getDecorView();
        root.addView(v, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        v.requestFocus();
        v.mAnim.start();
    }

    private void finish() {
        if (mDone)
            return;
        mDone = true;
        mAnim.cancel();
        ViewGroup parent = (ViewGroup) getParent();
        if (parent != null)
            parent.removeView(this);
        if (mOnDone != null)
            mOnDone.run();
    }

    /** salto: si va direttamente alla dissolvenza finale */
    private void skip() {
        if (mT < DURATION - 400)
            mAnim.setCurrentPlayTime(DURATION - 400);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (e.getAction() == MotionEvent.ACTION_DOWN)
            skip();
        return true;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        skip();
        return true;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN)
            skip();
        return true;
    }

    // 0..1 tra a e b (ms)
    private float seg(float a, float b) {
        float x = (mT - a) / (b - a);
        return x < 0 ? 0 : x > 1 ? 1 : x;
    }

    private static float easeOut(float x) {
        float y = 1 - x;
        return 1 - y * y * y;
    }

    private static float backOut(float x) {
        final float c = 1.70158f;
        float y = x - 1;
        return 1 + (c + 1) * y * y * y + c * y * y;
    }

    @Override
    protected void onDraw(Canvas c) {
        final int w = getWidth(), h = getHeight();
        final float u = Math.min(w, h) / 720f;
        final float fadeOut = 1f - seg(DURATION - 450, DURATION);
        final int alphaAll = (int) (255 * fadeOut);

        // sfondo: nero con un alone rosso scuro al centro che "respira"
        mPaint.setShader(null);
        mPaint.setColor(Color.argb(alphaAll, 8, 8, 10));
        c.drawRect(0, 0, w, h, mPaint);
        float pulse = 0.85f + 0.15f * (float) Math.sin(mT / 380.0);
        float bgIn = easeOut(seg(0, 900));
        mPaint.setShader(new RadialGradient(w / 2f, h / 2f, Math.max(w, h) * 0.55f,
            Color.argb((int) (90 * bgIn * pulse * fadeOut), 150, 20, 16), Color.TRANSPARENT, Shader.TileMode.CLAMP));
        c.drawRect(0, 0, w, h, mPaint);
        mPaint.setShader(null);

        // tre graffi diagonali che tagliano lo schermo (0 - 650 ms), poi restano tenui
        final float cx = w / 2f, cy = h / 2f - 40 * u;
        for (int i = 0; i < 3; ++i) {
            float start = 60 + i * 110;
            float p = easeOut(seg(start, start + 380));
            if (p <= 0)
                continue;
            float off = (i - 1) * 70 * u;
            float x0 = cx - 520 * u + off, y0 = cy - 300 * u;
            float x1 = cx + 380 * u + off, y1 = cy + 330 * u;
            float xe = x0 + (x1 - x0) * p, ye = y0 + (y1 - y0) * p;
            float fade = 1f - 0.75f * seg(start + 500, start + 1300);
            int a = (int) (230 * fade * fadeOut);
            mGlow.setStyle(Paint.Style.STROKE);
            mGlow.setStrokeCap(Paint.Cap.ROUND);
            mGlow.setStrokeWidth((10 - i * 2) * u + 6 * u);
            mGlow.setMaskFilter(new BlurMaskFilter(18 * u, BlurMaskFilter.Blur.NORMAL));
            mGlow.setColor(Color.argb(a / 2, 255, 40, 30));
            c.drawLine(x0, y0, xe, ye, mGlow);
            mGlow.setMaskFilter(null);
            mGlow.setStrokeWidth((7 - i * 1.5f) * u);
            mGlow.setShader(new LinearGradient(x0, y0, xe, ye, Color.argb(0, 222, 52, 40), Color.argb(a, 255, 90, 70),
                Shader.TileMode.CLAMP));
            c.drawLine(x0, y0, xe, ye, mGlow);
            mGlow.setShader(null);
        }

        // logo: entra con rimbalzo (500 - 1150 ms), poi un bagliore che scorre
        float lp = seg(500, 1150);
        if (lp > 0 && mLogo != null) {
            float s = 0.6f + 0.4f * backOut(lp);
            float lh = 210 * u * s, lw = lh * mLogo.getWidth() / mLogo.getHeight();
            RectF dst = new RectF(cx - lw / 2, cy - lh / 2, cx + lw / 2, cy + lh / 2);
            // ombra rossa sfocata dietro
            mGlow.setStyle(Paint.Style.FILL);
            mGlow.setMaskFilter(new BlurMaskFilter(40 * u, BlurMaskFilter.Blur.NORMAL));
            mGlow.setColor(Color.argb((int) (110 * lp * pulse * fadeOut), 200, 30, 24));
            c.drawOval(new RectF(dst.left + lw * 0.1f, dst.top + lh * 0.15f, dst.right - lw * 0.1f, dst.bottom - lh * 0.1f), mGlow);
            mGlow.setMaskFilter(null);
            mPaint.setAlpha((int) (255 * Math.min(1f, lp * 1.6f) * fadeOut));
            c.drawBitmap(mLogo, new Rect(0, 0, mLogo.getWidth(), mLogo.getHeight()), dst, mPaint);
            mPaint.setAlpha(255);
            // riflesso che attraversa il logo (1300 - 1900 ms)
            float sp = seg(1300, 1900);
            if (sp > 0 && sp < 1) {
                int save = c.save();
                c.clipRect(dst);
                float sx = dst.left - lw * 0.4f + (lw * 1.8f) * sp;
                mPaint.setShader(new LinearGradient(sx - 40 * u, 0, sx + 40 * u, 0,
                    new int[] { Color.TRANSPARENT, Color.argb((int) (120 * fadeOut), 255, 255, 255), Color.TRANSPARENT }, null,
                    Shader.TileMode.CLAMP));
                c.drawRect(dst, mPaint);
                mPaint.setShader(null);
                c.restoreToCount(save);
            }
        }

        // titolo lettera per lettera (1000 - 1600 ms)
        final String title = "T5JOOL";
        mText.setTypeface(Typeface.create("sans-serif-condensed", Typeface.BOLD));
        mText.setTextSize(64 * u);
        mText.setLetterSpacing(0.25f);
        float tw = mText.measureText(title);
        float tx = cx - tw / 2, ty = cy + 170 * u;
        for (int i = 0; i < title.length(); ++i) {
            float p = easeOut(seg(1000 + i * 70, 1300 + i * 70));
            if (p <= 0)
                break;
            String ch = title.substring(i, i + 1);
            float x = tx + mText.measureText(title.substring(0, i));
            mText.setColor(Color.argb((int) (255 * p * fadeOut), 240, 240, 240));
            c.drawText(ch, x, ty + (1 - p) * 24 * u, mText);
        }

        // sottotitolo e linea rossa
        float sp = easeOut(seg(1500, 1950));
        if (sp > 0) {
            mText.setTypeface(Typeface.create("sans-serif-light", Typeface.NORMAL));
            mText.setLetterSpacing(0.12f);
            mText.setTextSize(22 * u);
            String sub = "BLACK OPS ZOMBIES RECOMP";
            mText.setColor(Color.argb((int) (200 * sp * fadeOut), 200, 200, 205));
            c.drawText(sub, cx - mText.measureText(sub) / 2, ty + 44 * u, mText);
            float lw = 260 * u * sp;
            mPaint.setColor(Color.argb((int) (255 * fadeOut), 222, 52, 40));
            c.drawRect(cx - lw / 2, ty + 62 * u, cx + lw / 2, ty + 65 * u, mPaint);
        }

        // in basso: versione e sviluppatore, barra di caricamento
        float bp = easeOut(seg(1800, 2300));
        if (bp > 0) {
            mText.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
            mText.setLetterSpacing(0.05f);
            mText.setTextSize(17 * u);
            mText.setColor(Color.argb((int) (170 * bp * fadeOut), 170, 170, 176));
            String left = mVersion.isEmpty() ? "" : "v" + mVersion;
            String right = "Developed by Joolace";
            c.drawText(left, 40 * u, h - 34 * u, mText);
            c.drawText(right, w - 40 * u - mText.measureText(right), h - 34 * u, mText);
        }
        float lpBar = seg(300, DURATION - 500);
        mPaint.setColor(Color.argb((int) (60 * fadeOut), 255, 255, 255));
        c.drawRect(0, h - 4 * u, w, h, mPaint);
        mPaint.setColor(Color.argb((int) (255 * fadeOut), 222, 52, 40));
        c.drawRect(0, h - 4 * u, w * easeOut(lpBar), h, mPaint);
    }
}

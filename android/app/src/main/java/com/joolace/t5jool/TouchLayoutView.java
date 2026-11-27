package com.joolace.t5jool;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

import java.io.File;
import java.io.FileWriter;
import java.io.BufferedReader;
import java.io.FileReader;
import java.util.Locale;

/**
 * Editor della disposizione dei comandi touch nel lanciatore: stesso file e stesso formato dell'editor nel gioco
 * (src/ui/bo1_touch.cpp): <home>/t5jool/touch_layout.txt, righe "indice x y scala" (x, y = centro in frazioni dello
 * schermo, indice 11 = levetta). Posizioni predefinite copiate da s_buttons[] di bo1_touch.cpp.
 */
final class TouchLayoutView extends View {
    interface Listener { void onDone(); }

    // { cx, cy, r, top } in unita' da 720p, cx dal bordo destro (negativo = dal sinistro), cy dal basso (o dall'alto)
    private static final float[][] BUTTONS = {
        { 150, 165, 76, 0 }, { 292, 150, 48, 0 }, { 252, 268, 40, 0 }, { 150, 318, 44, 0 }, { 52, 268, 38, 0 },
        { 270, 52, 34, 0 }, { 405, 240, 36, 0 }, { 418, 128, 36, 0 }, { 395, 42, 30, 0 }, { 52, 48, 28, 1 },
        { -52, 48, 28, 1 },
    };
    private static final String[] LABELS = { "FIRE", "AIM", "RELOAD", "JUMP", "CROUCH", "SWITCH", "KNIFE", "LETHAL",
        "TACTICAL", "PAUSE", "SCORE", "MOVE" };
    private static final int STICK = 11, COUNT = 12;
    private static final int ACCENT = Color.rgb(222, 52, 40);
    private static final String[] TOOLS = { "-", "+", "Reset", "Done" };

    private final float mTouchScale;
    private final File mFile;
    private final Listener mListener;
    private final boolean[] mMoved = new boolean[COUNT];
    private final float[] mNx = new float[COUNT], mNy = new float[COUNT], mScale = new float[COUNT];
    private int mSelected = -1;
    private int mDragId = -1, mPinchId = -1;
    private float mOffX, mOffY, mPinchDist0, mPinchScale0, mDragX, mDragY;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF[] mToolRects = new RectF[TOOLS.length];

    TouchLayoutView(Context ctx, float touchScale, Listener l) {
        super(ctx);
        mTouchScale = touchScale;
        mListener = l;
        mFile = new File(T5Settings.root(), "home/t5jool/touch_layout.txt");
        reset();
        load();
        setBackgroundColor(Color.rgb(16, 14, 15));
        for (int i = 0; i < TOOLS.length; ++i)
            mToolRects[i] = new RectF();
    }

    private void reset() {
        for (int i = 0; i < COUNT; ++i) {
            mMoved[i] = false;
            mScale[i] = 1.0f;
        }
    }

    private void load() {
        if (!mFile.isFile())
            return;
        try (BufferedReader r = new BufferedReader(new FileReader(mFile))) {
            String line;
            while ((line = r.readLine()) != null) {
                String[] p = line.trim().split("\\s+");
                if (p.length < 4)
                    continue;
                int i = Integer.parseInt(p[0]);
                if (i < 0 || i >= COUNT)
                    continue;
                float x = Float.parseFloat(p[1]), y = Float.parseFloat(p[2]), sc = Float.parseFloat(p[3]);
                mMoved[i] = x >= 0;
                mNx[i] = x;
                mNy[i] = y;
                mScale[i] = sc > 0.3f && sc < 3.0f ? sc : 1.0f;
            }
        } catch (Exception ignored) {
        }
    }

    void save() {
        mFile.getParentFile().mkdirs();
        try (FileWriter w = new FileWriter(mFile)) {
            for (int i = 0; i < COUNT; ++i)
                if (mMoved[i] || mScale[i] != 1.0f)
                    w.write(String.format(Locale.US, "%d %.4f %.4f %.3f\n", i, mMoved[i] ? mNx[i] : -1.0f, mNy[i], mScale[i]));
        } catch (Exception ignored) {
        }
    }

    private float u() {
        return getHeight() / 720.0f * mTouchScale;
    }

    /** centro e raggio: {x, y, r} */
    private float[] item(int i) {
        float W = getWidth(), H = getHeight(), u = u();
        float x, y, r;
        if (i == STICK) {
            x = 210 * u;
            y = H - 200 * u;
            r = 110 * u;
        } else {
            float[] b = BUTTONS[i];
            x = b[0] >= 0 ? W - b[0] * u : -b[0] * u;
            y = b[3] != 0 ? b[1] * u : H - b[1] * u;
            r = b[2] * u;
        }
        if (mMoved[i]) {
            x = mNx[i] * W;
            y = mNy[i] * H;
        }
        return new float[] { x, y, r * mScale[i] };
    }

    private int itemAt(float x, float y) {
        for (int i = 0; i < COUNT; ++i) {
            float[] c = item(i);
            float rr = c[2] * 1.15f;
            if ((x - c[0]) * (x - c[0]) + (y - c[1]) * (y - c[1]) <= rr * rr)
                return i;
        }
        return -1;
    }

    private void layoutTools() {
        float s = getHeight() / 720.0f;
        float[] widths = { 70, 70, 130, 130 };
        float total = 0;
        for (float w : widths)
            total += (w + 14) * s;
        float x = (getWidth() - total) / 2;
        for (int i = 0; i < TOOLS.length; ++i) {
            mToolRects[i].set(x, 20 * s, x + widths[i] * s, 76 * s);
            x += (widths[i] + 14) * s;
        }
    }

    private void rescale(int i, float f) {
        if (i < 0)
            return;
        mScale[i] = Math.max(0.5f, Math.min(2.5f, mScale[i] * f));
    }

    @Override
    protected void onDraw(Canvas c) {
        float s = getHeight() / 720.0f;
        layoutTools();
        mPaint.setTextAlign(Paint.Align.CENTER);
        for (int i = 0; i < COUNT; ++i) {
            float[] it = item(i);
            boolean sel = i == mSelected;
            if (sel) {
                mPaint.setStyle(Paint.Style.FILL);
                mPaint.setColor(Color.argb(70, 255, 80, 60));
                c.drawCircle(it[0], it[1], it[2] * 1.4f, mPaint);
            }
            mPaint.setStyle(Paint.Style.FILL);
            mPaint.setColor(sel ? Color.argb(220, 222, 52, 40) : Color.argb(200, 20, 20, 24));
            c.drawCircle(it[0], it[1], it[2], mPaint);
            mPaint.setStyle(Paint.Style.STROKE);
            mPaint.setStrokeWidth(3 * s);
            mPaint.setColor(sel ? Color.rgb(255, 120, 100) : Color.argb(200, 255, 255, 255));
            c.drawCircle(it[0], it[1], it[2], mPaint);
            if (i == STICK) {
                mPaint.setStyle(Paint.Style.FILL);
                mPaint.setColor(Color.argb(150, 255, 255, 255));
                c.drawCircle(it[0], it[1], it[2] * 0.42f, mPaint);
            }
            mPaint.setStyle(Paint.Style.FILL);
            mPaint.setColor(Color.WHITE);
            mPaint.setTextSize(Math.max(11 * s, Math.min(20 * s, it[2] * 0.36f)));
            mPaint.setFakeBoldText(true);
            c.drawText(LABELS[i], it[0], it[1] + mPaint.getTextSize() * 0.35f, mPaint);
        }
        for (int t = 0; t < TOOLS.length; ++t) {
            boolean off = t < 2 && mSelected < 0;
            mPaint.setStyle(Paint.Style.FILL);
            mPaint.setColor(t == 3 ? ACCENT : Color.rgb(40, 38, 40));
            mPaint.setAlpha(off ? 100 : 235);
            c.drawRoundRect(mToolRects[t], 10 * s, 10 * s, mPaint);
            mPaint.setColor(Color.WHITE);
            mPaint.setAlpha(off ? 110 : 255);
            mPaint.setTextSize(28 * s);
            c.drawText(TOOLS[t], mToolRects[t].centerX(), mToolRects[t].centerY() + 10 * s, mPaint);
        }
        mPaint.setAlpha(255);
        mPaint.setColor(Color.argb(210, 255, 255, 255));
        mPaint.setFakeBoldText(false);
        mPaint.setTextSize(20 * s);
        c.drawText("Drag to move  -  pinch or -/+ to resize  -  Done to save", getWidth() / 2f, 112 * s, mPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int idx = e.getActionIndex();
        int id = e.getPointerId(idx);
        float x = e.getX(idx), y = e.getY(idx);
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                if (mDragId != -1 && mSelected >= 0 && mPinchId == -1) {
                    mPinchId = id;
                    mPinchDist0 = (float) Math.hypot(x - mDragX, y - mDragY);
                    mPinchScale0 = mScale[mSelected];
                    return true;
                }
                for (int t = 0; t < TOOLS.length; ++t) {
                    if (mToolRects[t].contains(x, y)) {
                        if (t == 0)
                            rescale(mSelected, 1 / 1.1f);
                        else if (t == 1)
                            rescale(mSelected, 1.1f);
                        else if (t == 2) {
                            reset();
                            mSelected = -1;
                        } else {
                            save();
                            if (mListener != null)
                                mListener.onDone();
                        }
                        invalidate();
                        return true;
                    }
                }
                mSelected = itemAt(x, y);
                if (mSelected >= 0) {
                    float[] it = item(mSelected);
                    mDragId = id;
                    mOffX = it[0] - x;
                    mOffY = it[1] - y;
                    mDragX = x;
                    mDragY = y;
                }
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_MOVE:
                for (int p = 0; p < e.getPointerCount(); ++p) {
                    int pid = e.getPointerId(p);
                    float px = e.getX(p), py = e.getY(p);
                    if (pid == mDragId) {
                        mDragX = px;
                        mDragY = py;
                    }
                    if (pid == mPinchId && mSelected >= 0 && mPinchDist0 > 1) {
                        float d = (float) Math.hypot(px - mDragX, py - mDragY);
                        mScale[mSelected] = mPinchScale0;
                        rescale(mSelected, d / mPinchDist0);
                    } else if (pid == mDragId && mSelected >= 0 && mPinchId == -1) {
                        mMoved[mSelected] = true;
                        mNx[mSelected] = Math.max(0.02f, Math.min(0.98f, (px + mOffX) / getWidth()));
                        mNy[mSelected] = Math.max(0.02f, Math.min(0.98f, (py + mOffY) / getHeight()));
                    }
                }
                invalidate();
                return true;
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (id == mDragId || e.getActionMasked() != MotionEvent.ACTION_POINTER_UP) {
                    mDragId = -1;
                    mPinchId = -1;
                } else if (id == mPinchId)
                    mPinchId = -1;
                return true;
        }
        return super.onTouchEvent(e);
    }
}

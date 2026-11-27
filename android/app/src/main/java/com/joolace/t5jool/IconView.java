package com.joolace.t5jool;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.View;

/** Icone semplici disegnate a mano (nessuna risorsa): processore, GPU, RAM, sistema, schermo, memoria, Discord. */
public class IconView extends View {
    public static final int CPU = 0, GPU = 1, RAM = 2, OS = 3, SCREEN = 4, STORAGE = 5, DISCORD = 6, DRIVER = 7, PLAY = 8, BUG = 9, SAVE = 10, IMAGE = 11;

    private final int mKind;
    private final int mColor;
    private final Paint mStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mFill = new Paint(Paint.ANTI_ALIAS_FLAG);

    public IconView(Context ctx, int kind, int color) {
        super(ctx);
        mKind = kind;
        mColor = color;
        mStroke.setStyle(Paint.Style.STROKE);
        mStroke.setStrokeCap(Paint.Cap.ROUND);
        mStroke.setStrokeJoin(Paint.Join.ROUND);
        mStroke.setColor(color);
        mFill.setStyle(Paint.Style.FILL);
        mFill.setColor(color);
    }

    @Override
    protected void onDraw(Canvas c) {
        final float s = Math.min(getWidth(), getHeight());
        final float ox = (getWidth() - s) / 2f, oy = (getHeight() - s) / 2f;
        c.save();
        c.translate(ox, oy);
        c.scale(s / 24f, s / 24f); // disegno su una griglia 24x24
        mStroke.setStrokeWidth(1.8f);
        switch (mKind) {
            case CPU: {
                c.drawRoundRect(new RectF(6, 6, 18, 18), 2, 2, mStroke);
                c.drawRect(new RectF(9.5f, 9.5f, 14.5f, 14.5f), mFill);
                for (int i = 0; i < 3; ++i) {
                    float p = 9 + i * 3;
                    c.drawLine(p, 2.5f, p, 6, mStroke);
                    c.drawLine(p, 18, p, 21.5f, mStroke);
                    c.drawLine(2.5f, p, 6, p, mStroke);
                    c.drawLine(18, p, 21.5f, p, mStroke);
                }
                break;
            }
            case GPU: {
                c.drawRoundRect(new RectF(2.5f, 6, 21.5f, 17), 2, 2, mStroke);
                c.drawCircle(9, 11.5f, 3.2f, mStroke);
                c.drawCircle(9, 11.5f, 0.9f, mFill);
                c.drawLine(14.5f, 9.5f, 19, 9.5f, mStroke);
                c.drawLine(14.5f, 13.5f, 19, 13.5f, mStroke);
                c.drawLine(5, 17, 5, 20, mStroke);
                c.drawLine(9, 17, 9, 20, mStroke);
                break;
            }
            case RAM: {
                c.drawRoundRect(new RectF(2.5f, 7, 21.5f, 15.5f), 1.5f, 1.5f, mStroke);
                for (int i = 0; i < 4; ++i)
                    c.drawRect(new RectF(5 + i * 4, 9.3f, 7.4f + i * 4, 13.2f), mFill);
                for (int i = 0; i < 6; ++i)
                    c.drawLine(4.5f + i * 3, 15.5f, 4.5f + i * 3, 18.5f, mStroke);
                break;
            }
            case OS: {
                // robottino di Android
                c.drawArc(new RectF(5.5f, 5.5f, 18.5f, 17), 180, 180, true, mFill);
                c.drawRoundRect(new RectF(5.5f, 12.4f, 18.5f, 21), 2, 2, mFill);
                c.drawLine(8, 4, 9.4f, 6.4f, mStroke);
                c.drawLine(16, 4, 14.6f, 6.4f, mStroke);
                Paint eye = new Paint(Paint.ANTI_ALIAS_FLAG);
                eye.setColor(0xFF1C1414);
                c.drawCircle(9.6f, 9.4f, 1, eye);
                c.drawCircle(14.4f, 9.4f, 1, eye);
                break;
            }
            case SCREEN: {
                c.drawRoundRect(new RectF(2.5f, 4.5f, 21.5f, 16.5f), 2, 2, mStroke);
                c.drawLine(12, 16.5f, 12, 19.5f, mStroke);
                c.drawLine(8, 20, 16, 20, mStroke);
                break;
            }
            case STORAGE: {
                Path p = new Path();
                p.moveTo(8, 2.5f);
                p.lineTo(18, 2.5f);
                p.lineTo(19.5f, 4);
                p.lineTo(19.5f, 21.5f);
                p.lineTo(4.5f, 21.5f);
                p.lineTo(4.5f, 6);
                p.close();
                c.drawPath(p, mStroke);
                for (int i = 0; i < 4; ++i)
                    c.drawLine(9 + i * 2.6f, 5.5f, 9 + i * 2.6f, 9, mStroke);
                break;
            }
            case DRIVER: {
                // ingranaggio
                c.drawCircle(12, 12, 3, mStroke);
                for (int i = 0; i < 8; ++i) {
                    double a = Math.PI * i / 4;
                    c.drawLine(12 + (float) Math.cos(a) * 5.5f, 12 + (float) Math.sin(a) * 5.5f,
                        12 + (float) Math.cos(a) * 8.5f, 12 + (float) Math.sin(a) * 8.5f, mStroke);
                }
                c.drawCircle(12, 12, 6, mStroke);
                break;
            }
            case PLAY: {
                Path p = new Path();
                p.moveTo(7.5f, 4.5f);
                p.lineTo(19.5f, 12);
                p.lineTo(7.5f, 19.5f);
                p.close();
                c.drawPath(p, mFill);
                break;
            }
            case BUG: {
                c.drawOval(new RectF(7.5f, 8, 16.5f, 20), mStroke);
                c.drawArc(new RectF(9, 4.5f, 15, 10.5f), 180, 180, false, mStroke);
                c.drawLine(12, 10, 12, 20, mStroke);
                c.drawLine(4, 11, 7.6f, 12.5f, mStroke);
                c.drawLine(20, 11, 16.4f, 12.5f, mStroke);
                c.drawLine(3.5f, 16, 7.5f, 15.5f, mStroke);
                c.drawLine(20.5f, 16, 16.5f, 15.5f, mStroke);
                break;
            }
            case SAVE: {
                Path p = new Path();
                p.moveTo(4, 4);
                p.lineTo(17, 4);
                p.lineTo(20, 7);
                p.lineTo(20, 20);
                p.lineTo(4, 20);
                p.close();
                c.drawPath(p, mStroke);
                c.drawRect(new RectF(8, 4, 15, 9), mFill);
                c.drawRoundRect(new RectF(7, 13, 17, 20), 1, 1, mStroke);
                break;
            }
            case IMAGE: {
                c.drawRoundRect(new RectF(3, 5, 21, 19), 2, 2, mStroke);
                c.drawCircle(8.5f, 9.5f, 1.6f, mFill);
                Path p = new Path();
                p.moveTo(4, 18);
                p.lineTo(10, 12);
                p.lineTo(14, 16);
                p.lineTo(16.5f, 13.5f);
                p.lineTo(20, 17);
                c.drawPath(p, mStroke);
                break;
            }
            case DISCORD: {
                // la faccina del controller di Discord, semplificata
                Path p = new Path();
                p.moveTo(6.2f, 5.6f);
                p.cubicTo(8, 4.8f, 9.6f, 4.5f, 10.2f, 4.4f);
                p.lineTo(10.7f, 5.4f);
                p.cubicTo(11.6f, 5.3f, 12.4f, 5.3f, 13.3f, 5.4f);
                p.lineTo(13.8f, 4.4f);
                p.cubicTo(14.4f, 4.5f, 16, 4.8f, 17.8f, 5.6f);
                p.cubicTo(20.3f, 9.3f, 21, 12.9f, 20.6f, 16.5f);
                p.cubicTo(19, 17.7f, 17.4f, 18.4f, 15.9f, 18.9f);
                p.lineTo(15, 17.4f);
                p.cubicTo(15.7f, 17.1f, 16.3f, 16.8f, 16.9f, 16.4f);
                p.cubicTo(13.7f, 17.9f, 10.3f, 17.9f, 7.1f, 16.4f);
                p.cubicTo(7.7f, 16.8f, 8.3f, 17.1f, 9, 17.4f);
                p.lineTo(8.1f, 18.9f);
                p.cubicTo(6.6f, 18.4f, 5, 17.7f, 3.4f, 16.5f);
                p.cubicTo(3, 12.3f, 4, 8.8f, 6.2f, 5.6f);
                p.close();
                c.drawPath(p, mFill);
                // occhi: viola sul logo bianco (banner), scuri sul logo viola (pulsante della home)
                Paint eye = new Paint(Paint.ANTI_ALIAS_FLAG);
                eye.setColor(mColor == 0xFFFFFFFF ? 0xFF5865F2 : 0xFF1C1414);
                c.drawOval(new RectF(7.7f, 10.6f, 10.5f, 13.9f), eye);
                c.drawOval(new RectF(13.5f, 10.6f, 16.3f, 13.9f), eye);
                break;
            }
        }
        c.restore();
    }
}

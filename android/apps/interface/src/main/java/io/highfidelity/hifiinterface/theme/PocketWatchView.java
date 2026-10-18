package io.highfidelity.hifiinterface.theme;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.View;
import android.view.animation.LinearInterpolator;

/**
 * Waiting, in a Wonderland theme: the White Rabbit's pocket watch, its hand going round ("I'm late!"), in the theme's
 * colours. The plain spinner's stand-in for themes with "whimsy".
 */
public class PocketWatchView extends View {
    private static final long TURN_MSECS = 2400;
    private static final int TICKS = 12;

    private final Theme mTheme;
    private final Paint mCase = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mFace = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mTick = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mHand = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF mBow = new RectF();
    private float mAngle;
    private ValueAnimator mAnimator;

    public PocketWatchView(Context context, Theme theme) {
        super(context);
        mTheme = theme;
        float density = getResources().getDisplayMetrics().density;
        mCase.setStyle(Paint.Style.STROKE);
        mCase.setStrokeWidth(3 * density);
        mCase.setColor(theme.primary);
        mFace.setStyle(Paint.Style.FILL);
        mFace.setColor(Theme.withAlpha(theme.primary, 0.12f));
        mTick.setStrokeWidth(2 * density);
        mTick.setStrokeCap(Paint.Cap.ROUND);
        mTick.setColor(theme.textSecondary);
        mHand.setStrokeWidth(3 * density);
        mHand.setStrokeCap(Paint.Cap.ROUND);
        mHand.setColor(theme.accent);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mAnimator = ValueAnimator.ofFloat(0, 360);
        mAnimator.setDuration(TURN_MSECS);
        mAnimator.setRepeatCount(ValueAnimator.INFINITE);
        mAnimator.setInterpolator(new LinearInterpolator());
        mAnimator.addUpdateListener(animation -> {
            // ticks round a sixtieth at a time, like a watch's second hand
            float angle = (float) animation.getAnimatedValue();
            mAngle = (float) Math.floor(angle / (360f / 60)) * (360f / 60);
            invalidate();
        });
        mAnimator.start();
    }

    @Override
    protected void onDetachedFromWindow() {
        if (mAnimator != null) {
            mAnimator.cancel();
        }
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float width = getWidth(), height = getHeight();
        float radius = Math.min(width, height * 0.82f) / 2 - mCase.getStrokeWidth();
        float cx = width / 2, cy = height - radius - mCase.getStrokeWidth();

        // the bow and crown on top
        float bow = radius * 0.28f;
        mBow.set(cx - bow, cy - radius - bow * 2.1f, cx + bow, cy - radius - bow * 0.1f);
        canvas.drawOval(mBow, mCase);
        canvas.drawLine(cx, cy - radius - bow * 0.1f, cx, cy - radius, mCase);

        canvas.drawCircle(cx, cy, radius, mFace);
        canvas.drawCircle(cx, cy, radius, mCase);
        for (int i = 0; i < TICKS; i++) {
            double a = Math.toRadians(i * 360.0 / TICKS);
            float inner = radius * (i % 3 == 0 ? 0.70f : 0.80f);
            float outer = radius * 0.88f;
            canvas.drawLine(cx + (float) Math.sin(a) * inner, cy - (float) Math.cos(a) * inner,
                    cx + (float) Math.sin(a) * outer, cy - (float) Math.cos(a) * outer, mTick);
        }
        double a = Math.toRadians(mAngle);
        canvas.drawLine(cx, cy, cx + (float) Math.sin(a) * radius * 0.72f, cy - (float) Math.cos(a) * radius * 0.72f, mHand);
        canvas.drawCircle(cx, cy, mHand.getStrokeWidth(), mHand);
    }
}

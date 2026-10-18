package io.highfidelity.hifiinterface.theme;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

/**
 * A panel's frame in a theme's shape, drawn over it (a foreground): for "cut" corners, the top left and bottom right
 * cut off (painted in the colour behind the panel), the theme's border round it, and brackets on the other two
 * corners: the accent top right, the primary bottom left.
 */
public class PanelFrameDrawable extends Drawable {
    private static final float BRACKET_LENGTH_DP = 18;
    private static final float BRACKET_WIDTH_DP = 2;
    private static final float BORDER_WIDTH_DP = 1;

    private final Theme mTheme;
    private final int mBehindColor;
    private final float mDensity;
    private final Paint mMaskPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBracketPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path mPath = new Path();

    public PanelFrameDrawable(Theme theme, int behindColor, float density) {
        mTheme = theme;
        mBehindColor = behindColor;
        mDensity = density;
        mMaskPaint.setStyle(Paint.Style.FILL);
        mMaskPaint.setColor(behindColor);
        mBorderPaint.setStyle(Paint.Style.STROKE);
        mBorderPaint.setStrokeWidth(BORDER_WIDTH_DP * density);
        mBorderPaint.setColor(Theme.withAlpha(theme.border, theme.borderAlpha));
        mBracketPaint.setStyle(Paint.Style.STROKE);
        mBracketPaint.setStrokeWidth(BRACKET_WIDTH_DP * density);
        mBracketPaint.setStrokeCap(Paint.Cap.SQUARE);
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        float left = bounds.left, top = bounds.top, right = bounds.right, bottom = bounds.bottom;
        boolean cut = Theme.CORNERS_CUT.equals(mTheme.corners);
        float c = cut ? mTheme.cornerSizeDp * mDensity : 0;

        if (cut) {
            mPath.reset();
            mPath.moveTo(left, top);
            mPath.lineTo(left + c, top);
            mPath.lineTo(left, top + c);
            mPath.close();
            mPath.moveTo(right, bottom);
            mPath.lineTo(right - c, bottom);
            mPath.lineTo(right, bottom - c);
            mPath.close();
            canvas.drawPath(mPath, mMaskPaint);
        }

        if (mTheme.borderAlpha > 0) {
            float half = mBorderPaint.getStrokeWidth() / 2;
            mPath.reset();
            mPath.moveTo(left + c + half, top + half);
            mPath.lineTo(right - half, top + half);
            mPath.lineTo(right - half, bottom - c - half);
            mPath.lineTo(right - c - half, bottom - half);
            mPath.lineTo(left + half, bottom - half);
            mPath.lineTo(left + half, top + c + half);
            mPath.close();
            canvas.drawPath(mPath, mBorderPaint);
        }

        if (mTheme.brackets) {
            float length = BRACKET_LENGTH_DP * mDensity;
            float inset = mBracketPaint.getStrokeWidth() / 2;
            mBracketPaint.setColor(mTheme.accent);
            mPath.reset();
            mPath.moveTo(right - length, top + inset);
            mPath.lineTo(right - inset, top + inset);
            mPath.lineTo(right - inset, top + length);
            canvas.drawPath(mPath, mBracketPaint);
            mBracketPaint.setColor(mTheme.primary);
            mPath.reset();
            mPath.moveTo(left + inset, bottom - length);
            mPath.lineTo(left + inset, bottom - inset);
            mPath.lineTo(left + length, bottom - inset);
            canvas.drawPath(mPath, mBracketPaint);
        }
    }

    @Override
    public void setAlpha(int alpha) {}

    @Override
    public void setColorFilter(ColorFilter colorFilter) {}

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }
}

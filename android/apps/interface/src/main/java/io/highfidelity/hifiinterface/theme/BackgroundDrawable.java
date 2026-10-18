package io.highfidelity.hifiinterface.theme;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;

/**
 * A screen's background in a theme: its colour, and for the "grid" pattern a faint targeting grid with a glow of the
 * primary colour at the top (the Home Assistant Cyber HUD's background).
 */
public class BackgroundDrawable extends Drawable {
    private static final float GRID_SPACING_DP = 48;
    private static final float GRID_ALPHA = 0.035f;
    private static final float GLOW_ALPHA = 0.10f;

    private final Theme mTheme;
    private final float mDensity;
    private final Paint mGridPaint = new Paint();
    private final Paint mGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    public BackgroundDrawable(Theme theme, float density) {
        mTheme = theme;
        mDensity = density;
        mGridPaint.setColor(Theme.withAlpha(theme.primary, GRID_ALPHA));
        mGridPaint.setStrokeWidth(Math.max(1, density));
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        // an ellipse 80% of the width and half the height, centred just above the top
        float radiusX = bounds.width() * 0.8f;
        float radiusY = bounds.height() * 0.5f;
        RadialGradient glow = new RadialGradient(0, 0, radiusX,
                new int[] { Theme.withAlpha(mTheme.primary, GLOW_ALPHA), Theme.withAlpha(mTheme.primary, 0) },
                new float[] { 0, 0.7f }, Shader.TileMode.CLAMP);
        Matrix matrix = new Matrix();
        matrix.setScale(1, radiusY / radiusX);
        matrix.postTranslate(bounds.exactCenterX(), bounds.top - bounds.height() * 0.1f);
        glow.setLocalMatrix(matrix);
        mGlowPaint.setShader(glow);
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        canvas.drawColor(mTheme.background);
        if (!Theme.PATTERN_GRID.equals(mTheme.backgroundPattern)) {
            return;
        }
        canvas.drawRect(bounds, mGlowPaint);
        float spacing = GRID_SPACING_DP * mDensity;
        for (float x = bounds.left; x < bounds.right; x += spacing) {
            canvas.drawLine(x, bounds.top, x, bounds.bottom, mGridPaint);
        }
        for (float y = bounds.top; y < bounds.bottom; y += spacing) {
            canvas.drawLine(bounds.left, y, bounds.right, y, mGridPaint);
        }
    }

    @Override
    public void setAlpha(int alpha) {}

    @Override
    public void setColorFilter(ColorFilter colorFilter) {}

    @Override
    public int getOpacity() {
        return PixelFormat.OPAQUE;
    }
}

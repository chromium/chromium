// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.drawable;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A custom {@link Drawable} designed to render a rounded rectangular border within the drawable's
 * bounds. This class provides flexibility in defining the border's width, color, corner radius, and
 * insets. It leverages the Android drawing framework to efficiently draw this border on a {@link
 * Canvas}.
 */
@NullMarked
public class BorderDrawable extends Drawable {
    private final Paint mBorderPaint;
    private final int mInsets;
    private final float mBorderRadius;

    /**
     * Constructs a {@link BorderDrawable}.
     *
     * @param borderWidth The width of the border to draw, in pixels.
     * @param insets The insets to apply inside the bounds before drawing the border, in pixels.
     * @param borderColor The color of the border.
     * @param borderRadius The radius of the corners of the border, in pixels.
     */
    public BorderDrawable(
            int borderWidth, int insets, @ColorInt int borderColor, float borderRadius) {
        mInsets = insets;
        mBorderRadius = borderRadius;
        mBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mBorderPaint.setColor(borderColor);
        mBorderPaint.setStyle(Paint.Style.STROKE);
        mBorderPaint.setStrokeWidth(borderWidth);
    }

    @Override
    public void draw(Canvas canvas) {
        RectF bounds = new RectF(getBounds());
        bounds.inset(mInsets, mInsets);
        canvas.drawRoundRect(bounds, mBorderRadius, mBorderRadius, mBorderPaint);
    }

    @Override
    public void setAlpha(int alpha) {
        mBorderPaint.setAlpha(alpha);
        invalidateSelf();
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mBorderPaint.setColorFilter(colorFilter);
        invalidateSelf();
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }
}

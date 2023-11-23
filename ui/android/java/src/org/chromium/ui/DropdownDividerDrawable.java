// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

/** A drawable divider to be used by dropdown adapters. */
public class DropdownDividerDrawable extends Drawable {
    private final Paint mPaint;
    private final Rect mDividerRect;
    private final Integer mBackgroundColor;

    /**
     * Creates a drawable to draw a divider line that separates the list of {@link DropdownItem}
     * and, optionally, paints the rectangular canvas.
     * @param backgroundColor Popup background color. If {@code null}, does not paint the canvas.
     */
    public DropdownDividerDrawable(Integer backgroundColor) {
        mPaint = new Paint();
        mDividerRect = new Rect();
        mBackgroundColor = backgroundColor;
    }

    @Override
    public void draw(Canvas canvas) {
        if (mBackgroundColor != null) canvas.drawColor(mBackgroundColor);
        canvas.drawRect(mDividerRect, mPaint);
    }

    @Override
    public void onBoundsChange(Rect bounds) {
        mDividerRect.set(0, 0, bounds.width(), mDividerRect.height());
    }

    public void setHeight(int height) {
        mDividerRect.set(0, 0, mDividerRect.right, height);
    }

    public void setDividerColor(int color) {
        mPaint.setColor(color);
    }

    @Override
    public void setAlpha(int alpha) {}

    @Override
    public void setColorFilter(ColorFilter cf) {}

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSPARENT;
    }
}

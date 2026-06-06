// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.drawable;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.DrawableWrapper;
import android.graphics.drawable.GradientDrawable;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

/**
 * A drawable wrapper that overlays a dirty dot drawable in the top-right corner of a base drawable.
 */
@NullMarked
public class DirtyDotDrawableWrapper extends DrawableWrapper {
    private final Drawable mDotDrawable;
    private final int mDotSize;

    /**
     * Creates a new DirtyDotDrawableWrapper.
     *
     * @param baseDrawable The base drawable to be wrapped.
     * @param dotColor The color of the dirty dot.
     * @param dotSize The size of the dirty dot in pixels.
     */
    public DirtyDotDrawableWrapper(Drawable baseDrawable, @ColorInt int dotColor, int dotSize) {
        super(baseDrawable.mutate());
        mDotSize = dotSize;
        GradientDrawable dot = new GradientDrawable();
        dot.setShape(GradientDrawable.OVAL);
        dot.setColor(dotColor);
        mDotDrawable = dot;
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);
        mDotDrawable.draw(canvas);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);

        // Position the dot drawable in the top-right corner.
        int left = bounds.right - mDotSize;
        int top = bounds.top;
        int right = bounds.right;
        int bottom = bounds.top + mDotSize;
        mDotDrawable.setBounds(left, top, right, bottom);
    }

    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        boolean dotChanged = mDotDrawable.setVisible(visible, restart);
        boolean baseChanged = super.setVisible(visible, restart);
        return dotChanged || baseChanged;
    }

    @Override
    public void setAlpha(int alpha) {
        super.setAlpha(alpha);
        mDotDrawable.setAlpha(alpha);
    }
}

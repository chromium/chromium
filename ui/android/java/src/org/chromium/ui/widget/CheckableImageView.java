// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.Checkable;

import androidx.annotation.Nullable;

/**
 * ImageView that has checkable state. Checkable state can be used with StateListDrawable and
 * AnimatedStateListDrawable to dynamically change the appearance of this widget.
 */
public class CheckableImageView extends ChromeImageView implements Checkable {
    private static final int[] CHECKED_STATE_SET = {android.R.attr.state_checked};

    private boolean mChecked;

    public CheckableImageView(final Context context, final AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        if (!isChecked()) return super.onCreateDrawableState(extraSpace);
        int[] drawableState = super.onCreateDrawableState(extraSpace + CHECKED_STATE_SET.length);
        return mergeDrawableStates(drawableState, CHECKED_STATE_SET);
    }

    @Override
    public void setImageDrawable(@Nullable Drawable drawable) {
        if (drawable == getDrawable()) return;
        super.setImageDrawable(drawable);
        refreshDrawableState();
    }

    @Override
    public void toggle() {
        setChecked(!mChecked);
    }

    @Override
    public boolean isChecked() {
        return mChecked;
    }

    @Override
    public void setChecked(final boolean checked) {
        if (mChecked == checked) return;
        mChecked = checked;
        refreshDrawableState();
    }
}

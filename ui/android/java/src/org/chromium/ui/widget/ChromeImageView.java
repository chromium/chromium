// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.base.Log;

// TODO(crbug.com/40883889): This class has no use now, so we can get rid of it.
/**
 * A subclass of AppCompatImageView to add workarounds for bugs in Android Framework and Support
 * Library.
 */
public class ChromeImageView extends AppCompatImageView {
    private static final String TAG = "CIV";

    public ChromeImageView(Context context) {
        super(context);
    }

    public ChromeImageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ChromeImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        // Add extra method to stack and logging for https://crbug.com/1457791.
        final @Nullable Drawable drawable = getDrawable();
        if (drawable != null && drawable instanceof BitmapDrawable) {
            final @Nullable Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
            if (bitmap != null && bitmap.isRecycled()) {
                Log.e(TAG, "Trying to draw with recycled BitmapDrawable. Id: " + getId());
            }
        }
        super.onDraw(canvas);
    }
}

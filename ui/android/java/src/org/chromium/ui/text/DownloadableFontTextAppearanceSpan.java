// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import android.content.Context;
import android.content.res.Resources.NotFoundException;
import android.content.res.TypedArray;
import android.graphics.Typeface;
import android.text.TextPaint;
import android.text.style.TextAppearanceSpan;

import androidx.annotation.FontRes;
import androidx.annotation.StyleRes;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.ui.R;

/** {@link TextAppearanceSpan} that supports downloadable fonts, e.g. google sans. */
public class DownloadableFontTextAppearanceSpan extends TextAppearanceSpan {
    private final Typeface mTypeface;

    public DownloadableFontTextAppearanceSpan(Context context, int appearance) {
        super(context, appearance);
        TypedArray a =
                context.getTheme().obtainStyledAttributes(appearance, R.styleable.TextAppearance);
        Typeface temp;
        try {
            @FontRes
            int fontResId = a.getResourceId(R.styleable.TextAppearance_android_fontFamily, 0);
            temp = ResourcesCompat.getFont(context, fontResId);
        } catch (UnsupportedOperationException | NotFoundException e) {
            temp = null;
        }

        mTypeface = temp;
        a.recycle();
    }

    @Override
    public void updateMeasureState(TextPaint ds) {
        super.updateMeasureState(ds);
        if (mTypeface == null) return;
        @StyleRes int oldStyle = ds.getTypeface().getStyle();
        ds.setTypeface(Typeface.create(mTypeface, oldStyle));
    }
}

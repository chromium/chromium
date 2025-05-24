// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import android.content.Context;
import android.text.TextPaint;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.R;
import org.chromium.ui.util.AttrUtils;

/** Shows a blue clickable link with underlines turned on. */
@NullMarked
public class ChromeClickableSpan extends ClickableSpan {
    private final int mColor;
    private final Callback<View> mOnClick;
    private boolean mFocused;

    /**
     * @param context The {@link Context} used for accessing colors.
     * @param onClickCallback The callback notified when the span is clicked.
     */
    public ChromeClickableSpan(Context context, Callback<View> onClickCallback) {
        int defaultColor = context.getColor(R.color.default_text_color_link_baseline);
        mColor =
                AttrUtils.resolveColor(
                        context.getTheme(), R.attr.globalClickableSpanColor, defaultColor);
        mOnClick = onClickCallback;
    }

    /**
     * @param color The {@link ColorInt} of this clickable span.
     * @param onClickCallback The callback notified when the span is clicked.
     */
    public ChromeClickableSpan(@ColorInt int color, Callback<View> onClickCallback) {
        mColor = color;
        mOnClick = onClickCallback;
    }

    @Override
    public final void onClick(View view) {
        mOnClick.onResult(view);
    }

    // Enable underline on the link text.
    @Override
    public void updateDrawState(TextPaint textPaint) {
        super.updateDrawState(textPaint);
        textPaint.setColor(mColor);
        // When the span is focused, it means that it contains a background highlight. Remove the
        // text underline in this case.
        textPaint.setUnderlineText(!mFocused);
    }

    /**
     * Sets whether the span is focused.
     *
     * @param focused Whether the span is focused.
     */
    public void setFocused(boolean focused) {
        mFocused = focused;
    }

    /**
     * Determines whether the span is focused.
     *
     * @return {@code true} if the span is focused, {@code false} otherwise.
     */
    public boolean isFocused() {
        return mFocused;
    }
}

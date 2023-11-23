// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextPaint;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.ColorRes;

import org.chromium.base.Callback;
import org.chromium.ui.R;
import org.chromium.ui.util.AttrUtils;

/** Shows a blue clickable link with underlines turned off. */
public class NoUnderlineClickableSpan extends ClickableSpan {
    private final int mColor;
    private final Callback<View> mOnClick;

    /**
     * @param context The {@link Context} used for accessing colors.
     * @param onClickCallback The callback notified when the span is clicked.
     */
    public NoUnderlineClickableSpan(Context context, Callback<View> onClickCallback) {
        mColor =
                AttrUtils.resolveColor(
                        context.getTheme(),
                        R.attr.globalClickableSpanColor,
                        R.color.default_text_color_link_baseline);
        mOnClick = onClickCallback;
    }

    /**
     * @param context The {@link Resources} used for accessing colors.
     * @param colorResId The {@link ColorRes} of this clickable span.
     * @param onClickCallback The callback notified when the span is clicked.
     */
    public NoUnderlineClickableSpan(
            Context context, @ColorRes int colorResId, Callback<View> onClickCallback) {
        mColor = context.getColor(colorResId);
        mOnClick = onClickCallback;
    }

    @Override
    public final void onClick(View view) {
        mOnClick.onResult(view);
    }

    // Disable underline on the link text.
    @Override
    public void updateDrawState(TextPaint textPaint) {
        super.updateDrawState(textPaint);
        textPaint.setUnderlineText(false);
        textPaint.setColor(mColor);
    }
}

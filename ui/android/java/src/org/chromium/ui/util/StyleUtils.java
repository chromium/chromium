// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Typeface;
import android.text.TextPaint;

import androidx.annotation.StyleRes;
import androidx.annotation.StyleableRes;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.ui.R;

/** Helper functions for working with styles. */
public class StyleUtils {
    private static final String TAG = "StyleUtils";
    private static final int INVALID_RESOURCE_ID = -1;

    /**
     * Applies attributes extracted from a TextAppearance style to a TextPaint object.
     *
     * @param context The current Android context.
     * @param textPaint The {@link TextPaint} to apply the appearance to.
     * @param style The TextAppearance style resource to determine the TextPaint attribute values.
     * @param applyFontFamily Whether the font family defined in the style should be applied. The
     *     TextAppearance style may specify either a font resource (e.g. @font/accent_font) or a
     *     string (e.g. "sans-serif").
     * @param applyTextSize Whether the text size defined in the style should be applied.
     * @param applyTextColor Whether the text color defined in the style should be applied.
     */
    public static void applyTextAppearanceToTextPaint(
            Context context,
            TextPaint textPaint,
            @StyleRes int style,
            boolean applyFontFamily,
            boolean applyTextSize,
            boolean applyTextColor) {
        TypedArray appearance =
                context.getTheme().obtainStyledAttributes(style, R.styleable.TextAppearance);

        Typeface typeface;
        if (applyFontFamily) {
            @StyleableRes int fontStyleableRes = R.styleable.TextAppearance_android_fontFamily;
            int fontRes = appearance.getResourceId(fontStyleableRes, INVALID_RESOURCE_ID);
            if (fontRes != INVALID_RESOURCE_ID) {
                typeface = ResourcesCompat.getFont(context, fontRes);
            } else {
                String fontFamily = appearance.getString(fontStyleableRes);
                typeface = Typeface.create(fontFamily, Typeface.NORMAL);
            }

            textPaint.setTypeface(typeface);
        }

        if (applyTextSize) {
            float textSize =
                    appearance.getDimension(
                            R.styleable.TextAppearance_android_textSize, INVALID_RESOURCE_ID);
            assert textSize != INVALID_RESOURCE_ID : "textSize is not defined in style.";
            textPaint.setTextSize(textSize);
        }

        if (applyTextColor) {
            int textColor =
                    appearance.getColor(
                            R.styleable.TextAppearance_android_textColor, INVALID_RESOURCE_ID);
            assert textColor != INVALID_RESOURCE_ID : "textColor is not defined in style.";
            textPaint.setColor(textColor);
        }

        appearance.recycle();
    }
}

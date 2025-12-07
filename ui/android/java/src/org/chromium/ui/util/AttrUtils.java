// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.content.Context;
import android.content.res.Resources.Theme;
import android.util.TypedValue;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/** Helper functions for working with attributes. */
@NullMarked
public final class AttrUtils {
    /** Private constructor to stop instantiation. */
    private AttrUtils() {}

    /** Returns the given boolean attribute from the theme. */
    public static boolean resolveBoolean(Theme theme, @AttrRes int attrRes) {
        TypedValue typedValue = new TypedValue();
        theme.resolveAttribute(attrRes, typedValue, /* resolveRefs= */ true);
        return typedValue.data != 0;
    }

    /** Returns the given color attribute from the theme. */
    public static @ColorInt int resolveColor(Theme theme, @AttrRes int attrRes) {
        TypedValue typedValue = new TypedValue();
        theme.resolveAttribute(attrRes, typedValue, /* resolveRefs= */ true);
        if (typedValue.resourceId != 0) {
            // Color State List
            return theme.getResources().getColor(typedValue.resourceId, theme);
        } else {
            // Color Int
            return typedValue.data;
        }
    }

    /**
     * Returns the given color attribute from the theme or resolves and returns the given default
     * color if the attribute is not set in the theme.
     */
    public static @ColorInt int resolveColor(
            Theme theme, @AttrRes int attrRes, @ColorInt int defaultColor) {
        TypedValue typedValue = new TypedValue();
        if (theme.resolveAttribute(attrRes, typedValue, /* resolveRefs= */ true)) {
            return typedValue.data;
        } else {
            return defaultColor;
        }
    }

    /**
     * Resolves a dimension attribute from the theme and returns its value in pixels.
     *
     * @param context The context to resolve the theme attribute from.
     * @param dimenAttr The dimension attribute to resolve.
     * @return The dimension value in pixels, or -1 if the attribute is not defined in the theme.
     */
    public static @Px int getDimensionPixelSize(Context context, @AttrRes int dimenAttr) {
        var typedValue = new TypedValue();

        if (context.getTheme().resolveAttribute(dimenAttr, typedValue, true)) {
            return TypedValue.complexToDimensionPixelSize(
                    typedValue.data, context.getResources().getDisplayMetrics());
        }

        return -1;
    }
}

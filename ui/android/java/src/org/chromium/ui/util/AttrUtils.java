// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.content.res.Resources.Theme;
import android.util.TypedValue;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;

/** Helper functions for working with attributes. */
public final class AttrUtils {
    /** Private constructor to stop instantiation. */
    private AttrUtils() {}

    /** Returns the given boolean attribute from the theme. */
    public static boolean resolveBoolean(Theme theme, @AttrRes int attrRes) {
        TypedValue typedValue = new TypedValue();
        theme.resolveAttribute(attrRes, typedValue, /*resolveRefs=*/true);
        return typedValue.data != 0;
    }

    /** Returns the given color attribute from the theme. */
    public static @ColorInt int resolveColor(Theme theme, @AttrRes int attrRes) {
        TypedValue typedValue = new TypedValue();
        theme.resolveAttribute(attrRes, typedValue, /*resolveRefs=*/true);
        return typedValue.data;
    }
}
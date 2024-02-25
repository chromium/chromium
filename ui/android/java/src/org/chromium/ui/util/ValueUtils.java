// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.content.res.Resources;
import android.util.TypedValue;

import androidx.annotation.DimenRes;

/** Helper functions for working with {@TypedValue}s. */
public final class ValueUtils {

    /** Private constructor to stop instantiation. */
    private ValueUtils() {}

    /**
     * Gets the value of a float dimen. Can be replaced by Resources#getFloat once api level is 29+.
     *
     * @param resources {@link Resources} used to look up the dimen value.
     * @param dimenRes The dimen defined in a resource file.
     * @return The value as a float.
     */
    public static float getFloat(Resources resources, @DimenRes int dimenRes) {
        TypedValue typedValue = new TypedValue();
        resources.getValue(dimenRes, typedValue, true);
        return typedValue.getFloat();
    }
}

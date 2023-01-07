// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.shadows;

import android.content.Context;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.ui.util.ColorUtils;

/** Shadow class for {@link org.chromium.ui.util.ColorUtils} */
@Implements(ColorUtils.class)
public class ShadowColorUtils {
    public static boolean sInNightMode;

    @Implementation
    public static boolean inNightMode(Context context) {
        return sInNightMode;
    }
}

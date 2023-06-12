// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.util.DisplayMetrics;

/**
 * Helper functions relevant to working with displays, but have no parallel in the native
 * DisplayAndroid class.
 */
public abstract class DisplayUtil {
    public static final float UI_SCALING_FACTOR_FOR_AUTO = 1.34f;

    /**
     * @return The smaller of getDisplayWidth(), getDisplayHeight().
     */
    public static int getSmallestWidth(DisplayAndroid display) {
        int width = display.getDisplayWidth();
        int height = display.getDisplayHeight();
        return width < height ? width : height;
    }

    /**
     * @return The given value converted from px to dp.
     */
    public static int pxToDp(DisplayAndroid display, int value) {
        // Adding .5 is what Android does when doing this conversion.
        return (int) (value / display.getDipScale() + 0.5f);
    }

    /**
     * @return The given value converted from dp to px.
     */
    public static int dpToPx(DisplayAndroid display, int value) {
        // Adding .5 is what Android does when doing this conversion.
        return (int) (value * display.getDipScale() + 0.5f);
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for automotive devices.
     * @param displayMetrics The DisplayMetrics to scale up density for.
     * @return The DisplayMetrics that was scaled up.
     */
    public static DisplayMetrics scaleUpDisplayMetricsForAutomotive(DisplayMetrics displayMetrics) {
        displayMetrics.density *= UI_SCALING_FACTOR_FOR_AUTO;
        displayMetrics.densityDpi = (int) (displayMetrics.densityDpi * UI_SCALING_FACTOR_FOR_AUTO);
        displayMetrics.xdpi *= UI_SCALING_FACTOR_FOR_AUTO;
        displayMetrics.ydpi *= UI_SCALING_FACTOR_FOR_AUTO;
        return displayMetrics;
    }
}

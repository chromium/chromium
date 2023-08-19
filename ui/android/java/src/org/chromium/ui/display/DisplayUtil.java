// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.content.Context;
import android.content.res.Configuration;
import android.util.DisplayMetrics;
import android.view.WindowManager;

/**
 * Helper functions relevant to working with displays, but have no parallel in the native
 * DisplayAndroid class.
 */
public abstract class DisplayUtil {
    private static final float UI_SCALING_FACTOR_FOR_AUTOMOTIVE = 1.34f;
    private static float sUiScalingFactorForAutomotive = UI_SCALING_FACTOR_FOR_AUTOMOTIVE;

    /**
     * Change the UI scaling factor on automotive devices for testing.
     */
    public static void setUiScalingFactorForAutomotiveForTesting(float scalingFactor) {
        sUiScalingFactorForAutomotive = scalingFactor;
    }

    /**
     * Reset the UI scaling factor on automotive devices to the default value.
     */
    public static void resetUiScalingFactorForAutomotiveForTesting() {
        sUiScalingFactorForAutomotive = UI_SCALING_FACTOR_FOR_AUTOMOTIVE;
    }

    /**
     * Retrieves the UI scaling factor on automotive devices.
     */
    public static float getUiScalingFactorForAutomotive() {
        return sUiScalingFactorForAutomotive;
    }

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
        displayMetrics.density *= getUiScalingFactorForAutomotive();
        displayMetrics.densityDpi =
                (int) (displayMetrics.densityDpi * getUiScalingFactorForAutomotive());
        displayMetrics.xdpi *= getUiScalingFactorForAutomotive();
        displayMetrics.ydpi *= getUiScalingFactorForAutomotive();
        return displayMetrics;
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for automotive devices.
     * @param context The context used to retrieve the system {@link WindowManager}.
     * @param configuration The Configuration to scale up UI for.
     */
    public static void scaleUpConfigurationForAutomotive(
            Context context, Configuration configuration) {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        assert windowManager != null;
        windowManager.getDefaultDisplay().getRealMetrics(displayMetrics);

        configuration.densityDpi =
                (int) (displayMetrics.densityDpi * getUiScalingFactorForAutomotive());
        configuration.screenWidthDp = (int) (displayMetrics.widthPixels
                / (displayMetrics.density * getUiScalingFactorForAutomotive()));
        configuration.screenHeightDp = (int) (displayMetrics.heightPixels
                / (displayMetrics.density * getUiScalingFactorForAutomotive()));
        configuration.smallestScreenWidthDp =
                Math.min(configuration.screenWidthDp, configuration.screenHeightDp);
    }
}

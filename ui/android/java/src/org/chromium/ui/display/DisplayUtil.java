// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.annotation.Nullable;

/**
 * Helper functions relevant to working with displays, but have no parallel in the native
 * DisplayAndroid class.
 */
public abstract class DisplayUtil {
    private static @Nullable Float sUiScalingFactorForAutomotiveOverride;

    /** Change the UI scaling factor on automotive devices for testing. */
    public static void setUiScalingFactorForAutomotiveForTesting(float scalingFactor) {
        sUiScalingFactorForAutomotiveOverride = scalingFactor;
    }

    /** Reset the UI scaling factor on automotive devices to the default value. */
    public static void resetUiScalingFactorForAutomotiveForTesting() {
        sUiScalingFactorForAutomotiveOverride = null;
    }

    /**
     * Retrieves the UI scaling factor on automotive devices.
     * TODO: Remove this method and replace usages with getUiDensityForAutomotive.
     */
    @Deprecated
    public static float getUiScalingFactorForAutomotive() {
        return sUiScalingFactorForAutomotiveOverride;
    }

    public static int getUiDensityForAutomotive(Context context, int baseDensity) {
        TypedValue automotiveUiScaleFactor = new TypedValue();
        context.getResources()
                .getValue(
                        org.chromium.ui.R.dimen.automotive_ui_scale_factor,
                        automotiveUiScaleFactor,
                        true);
        float uiScalingFactor =
                sUiScalingFactorForAutomotiveOverride != null
                        ? sUiScalingFactorForAutomotiveOverride
                        : automotiveUiScaleFactor.getFloat();
        int rawScaledDensity = (int) (baseDensity * uiScalingFactor);
        // Round up to the nearest 20 to align with DisplayMetrics defined densities.
        return ((int) Math.ceil(rawScaledDensity / 20.0f)) * 20;
    }

    /** Returns the smaller of getDisplayWidth(), getDisplayHeight(). */
    public static int getSmallestWidth(DisplayAndroid display) {
        int width = display.getDisplayWidth();
        int height = display.getDisplayHeight();
        return width < height ? width : height;
    }

    /** Returns the given value converted from px to dp. */
    public static int pxToDp(DisplayAndroid display, int value) {
        // Adding .5 is what Android does when doing this conversion.
        return (int) (value / display.getDipScale() + 0.5f);
    }

    /** Returns the given value converted from dp to px. */
    public static int dpToPx(DisplayAndroid display, int value) {
        // Adding .5 is what Android does when doing this conversion.
        return (int) (value * display.getDipScale() + 0.5f);
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for automotive devices.
     *
     * @param displayMetrics The DisplayMetrics to scale up density for.
     * @return The DisplayMetrics that was scaled up.
     */
    public static DisplayMetrics scaleUpDisplayMetricsForAutomotive(
            Context context, DisplayMetrics displayMetrics) {
        int adjustedDensity = getUiDensityForAutomotive(context, displayMetrics.densityDpi);
        float scaling = (float) adjustedDensity / (float) displayMetrics.densityDpi;
        displayMetrics.density *= scaling;
        displayMetrics.densityDpi = adjustedDensity;
        displayMetrics.xdpi *= scaling;
        displayMetrics.ydpi *= scaling;
        return displayMetrics;
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for automotive devices.
     *
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

        int adjustedDensity = getUiDensityForAutomotive(context, displayMetrics.densityDpi);
        float scaling = (float) adjustedDensity / (float) displayMetrics.densityDpi;

        int screenWidthDp = displayMetrics.widthPixels;
        int screenHeightDp = displayMetrics.heightPixels;

        // Configuration.screenWidthDp and Configuration.screenHeightDp are not supposed to take
        // into account system bars. Since we are scaling up the UI in automotive during a time when
        // we cannot access the default Configuration (CompatActivity#attachBaseContext), we need
        // to manually subtract the system bar insets ourselves.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Insets systemBarInsets =
                    windowManager
                            .getCurrentWindowMetrics()
                            .getWindowInsets()
                            .getInsets(WindowInsets.Type.systemBars());
            screenHeightDp = screenHeightDp - systemBarInsets.top - systemBarInsets.bottom;
            screenWidthDp = screenWidthDp - systemBarInsets.left - systemBarInsets.right;
        }

        configuration.densityDpi = adjustedDensity;
        configuration.screenWidthDp =
                Math.round(screenWidthDp / (displayMetrics.density * scaling));
        configuration.screenHeightDp =
                Math.round(screenHeightDp / (displayMetrics.density * scaling));
        configuration.smallestScreenWidthDp =
                Math.min(configuration.screenWidthDp, configuration.screenHeightDp);
    }

    /**
     * Get current smallest screen width in dp. This method uses {@link WindowManager} on
     * Android R and above; otherwise, {@link DisplayUtil#getSmallestWidth(DisplayAndroid)}.
     *
     * @param context {@link Context} used to get system service and target display.
     * @return Smallest screen width in dp.
     */
    public static int getCurrentSmallestScreenWidth(Context context) {
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        // Android T does not receive updated width upon foldable unfold from window context.
        // Continue to rely on context on this case.
        Context windowManagerContext =
                (VERSION.SDK_INT >= VERSION_CODES.R && VERSION.SDK_INT < VERSION_CODES.TIRAMISU)
                        ? (display.getWindowContext() != null
                                ? display.getWindowContext()
                                : context)
                        : context;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // Context#getSystemService(Context.WINDOW_SERVICE) is preferred over
            // Activity#getWindowManager, because during #attachBaseContext, #getWindowManager
            // is not ready yet and always returns null. See crbug.com/1252150.
            WindowManager manager =
                    (WindowManager) windowManagerContext.getSystemService(Context.WINDOW_SERVICE);
            assert manager != null;
            Rect bounds = manager.getMaximumWindowMetrics().getBounds();
            return DisplayUtil.pxToDp(
                    display, Math.min(bounds.right - bounds.left, bounds.bottom - bounds.top));
        }
        return DisplayUtil.pxToDp(display, DisplayUtil.getSmallestWidth(display));
    }
}

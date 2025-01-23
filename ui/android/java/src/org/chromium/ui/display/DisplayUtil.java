// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import static org.chromium.build.NullUtil.assumeNonNull;

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

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.XrUtils;

/**
 * Helper functions relevant to working with displays, but have no parallel in the native
 * DisplayAndroid class.
 */
@NullMarked
public abstract class DisplayUtil {
    private static final String TAG = "DisplayUtil";
    private static @Nullable Float sUiScalingFactorForAutomotiveOverride;
    // For XR environment.
    private static @Nullable Float sUiScalingFactorForXrOverride;

    /** Returns true if the device requires UI scaling. */
    public static boolean isUiScaled() {
        return BuildInfo.getInstance().isAutomotive || XrUtils.isXrDevice();
    }

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
        return assumeNonNull(sUiScalingFactorForAutomotiveOverride);
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
        return scaleUpDisplayMetrics(adjustedDensity, displayMetrics);
    }

    private static DisplayMetrics scaleUpDisplayMetrics(
            int adjustedDensity, DisplayMetrics displayMetrics) {
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
        DisplayMetrics displayMetrics = getDisplayRealMetrics(context);

        int adjustedDensity = getUiDensityForAutomotive(context, displayMetrics.densityDpi);

        scaleUpConfigurationWithAdjustedDensity(
                context, displayMetrics, adjustedDensity, configuration);
    }

    private static DisplayMetrics getDisplayRealMetrics(Context context) {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        assert windowManager != null;
        windowManager.getDefaultDisplay().getRealMetrics(displayMetrics);
        return displayMetrics;
    }

    /**
     * Scale up the configuration {@link Configuration} based on the adjusted density dpi.
     *
     * @param context The context used to retrieve the system {@link WindowManager}.
     * @param displayMetrics The dsiplay metrics.
     * @param adjustedDensity The adjusted densityDpi for scaling up.
     * @param configuration The Configuration to scale up UI for.
     */
    private static void scaleUpConfigurationWithAdjustedDensity(
            Context context,
            DisplayMetrics displayMetrics,
            int adjustedDensity,
            Configuration configuration) {
        float scaling = (float) adjustedDensity / (float) displayMetrics.densityDpi;

        int screenWidthDp = displayMetrics.widthPixels;
        int screenHeightDp = displayMetrics.heightPixels;

        // Configuration.screenWidthDp and Configuration.screenHeightDp are not supposed to take
        // into account system bars. Since we are scaling up the UI in automotive during a time when
        // we cannot access the default Configuration (CompatActivity#attachBaseContext), we need
        // to manually subtract the system bar insets ourselves.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Insets systemBarInsets =
                    ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
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
     * Get current smallest screen width in dp.
     *
     * <p>This method uses {@link WindowManager} on Android R and above; otherwise, {@link
     * DisplayUtil#getSmallestWidth(DisplayAndroid)}.
     *
     * <p>This method raises an exception when it gets a context not associated with a display (e.g.
     * application contexts) and the strict mode is enabled. Use {@link
     * DisplayUtil#getCurrentSmallestScreenWidthAllowingFallback(Context)} if you want to fall back
     * to the default display in such cases.
     *
     * @param context {@link Context} used to get system service and target display.
     * @return Smallest screen width in dp.
     */
    public static int getCurrentSmallestScreenWidth(Context context) {
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            // Android T does not receive updated width upon foldable unfold from window context.
            // Continue to rely on context on this case.
            Context windowManagerContext =
                    (VERSION.SDK_INT >= VERSION_CODES.R && VERSION.SDK_INT < VERSION_CODES.TIRAMISU)
                            ? (display.getWindowContext() != null
                                    ? display.getWindowContext()
                                    : context)
                            : context;
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

    /**
     * Get current smallest screen width in dp.
     *
     * <p>This method is similar to {@link DisplayUtil#getCurrentSmallestScreenWidth(Context)}, but
     * it accepts contexts not associated with a display (e.g. application contexts). In such cases,
     * it returns the smallest width of the default display.
     *
     * <p>Do not use this method unless you need to fall back to the default display when the
     * current display is not available. It is undesirable in most cases.
     *
     * @param context {@link Context} used to get the current display.
     * @return Smallest screen width in dp.
     */
    public static int getCurrentSmallestScreenWidthAllowingFallback(Context context) {
        boolean isUiContext;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            isUiContext = context.isUiContext();
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            isUiContext = ContextUtils.activityFromContext(context) != null;
        } else {
            // On Android older than R, getCurrentSmallestScreenWidth behaves identically for UI
            // contexts and non-UI contexts.
            isUiContext = true;
        }
        if (isUiContext) {
            return getCurrentSmallestScreenWidth(context);
        }

        // Fall back to the default display. We do not use WindowManager because we can't obtain it
        // from non-UI contexts.
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        return DisplayUtil.pxToDp(display, DisplayUtil.getSmallestWidth(display));
    }

    /** Change the UI scaling factor on XR devices for testing. */
    static void setUiScalingFactorForXrForTesting(float scalingFactor) {
        sUiScalingFactorForXrOverride = scalingFactor;
    }

    /** Reset the UI scaling factor on XR devices to the default value. */
    static void resetUiScalingFactorForXrForTesting() {
        sUiScalingFactorForXrOverride = null;
    }

    private static float getUiScalingFactorForXrFromResource(Context context) {
        TypedValue xrUiScaleFactor = new TypedValue();
        context.getResources()
                .getValue(org.chromium.ui.R.dimen.xr_ui_scale_factor, xrUiScaleFactor, true);
        return xrUiScaleFactor.getFloat();
    }

    /** Get the density base on the UI scaling factor on XR devices. */
    public static int getUiDensityForXr(Context context, int baseDensity) {
        float uiScalingFactor =
                sUiScalingFactorForXrOverride != null
                        ? sUiScalingFactorForXrOverride
                        : getUiScalingFactorForXrFromResource(context);
        int rawScaledDensity = (int) (baseDensity * uiScalingFactor);
        // Round up to the nearest 10 to align with DisplayMetrics defined densities.
        return ((int) Math.ceil(rawScaledDensity / 10.0f)) * 10;
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for XR devices.
     *
     * @param context The context used to retrieve the scale value from {@link Resources}.
     * @param displayMetrics The DisplayMetrics whose density is to be scaled up.
     * @return The DisplayMetrics that was scaled up.
     */
    public static DisplayMetrics scaleUpDisplayMetricsForXr(
            Context context, DisplayMetrics displayMetrics) {
        int adjustedDensity = getUiDensityForXr(context, displayMetrics.densityDpi);

        return scaleUpDisplayMetrics(adjustedDensity, displayMetrics);
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for XR devices.
     *
     * @param context The context used to retrieve the system {@link WindowManager}.
     * @param configuration The Configuration to scale up UI for.
     */
    public static void scaleUpConfigurationForXr(Context context, Configuration configuration) {
        DisplayMetrics displayMetrics = getDisplayRealMetrics(context);

        int adjustedDensity = getUiDensityForXr(context, displayMetrics.densityDpi);

        scaleUpConfigurationWithAdjustedDensity(
                context, displayMetrics, adjustedDensity, configuration);
        Log.i(
                TAG,
                "SUI scaleUpConfigurationForXr Device ddpi="
                        + displayMetrics.densityDpi
                        + ". Updated: ddpi="
                        + configuration.densityDpi
                        + ", widthDp="
                        + configuration.screenWidthDp
                        + ", heightDp="
                        + configuration.screenHeightDp);
    }
}

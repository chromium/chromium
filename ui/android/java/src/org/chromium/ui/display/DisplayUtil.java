// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.util.TypedValue;
import android.view.Display;
import android.view.WindowInsets;
import android.view.WindowManager;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Helper functions relevant to working with displays, but have no parallel in the native
 * DisplayAndroid class.
 */
@NullMarked
public abstract class DisplayUtil {
    private static final String TAG = "DisplayUtil";
    // CaRMA phase 1 has version 1 and 2. Version 1 includes car-ready mobile app identification
    // logic, opaque blocking activity, and safe app area. Version 2 includes all v1 features plus
    // DPI Scaling.
    private static final String CARMA_PHASE_1_COMPLIANCE =
            "com.google.android.automotive.software.car_ready_mobile_apps";
    private static final String CARMA_DISPLAY_COMPAT_APP_META_DATA =
            "android.software.car.display_compatibility";
    private static @Nullable Boolean sCarmaPhase1Version2ComplianceForTesting;
    private static @Nullable Boolean sIsDisplayCompatAppForTesting;
    private static @Nullable Integer sSmallestScreenWidthForTesting;
    private static @Nullable Boolean sIsOnDefaultDisplayForTesting;
    private static @Nullable Float sUiScalingFactorForAutomotiveForTesting;
    // For XR environment.
    private static @Nullable Float sUiScalingFactorForXrForTesting;

    /** Returns true if the device requires UI scaling. */
    public static boolean isUiScaled() {
        return DeviceInfo.isAutomotive() || DeviceInfo.isXr();
    }

    /** Change the UI scaling factor on automotive devices for testing. */
    public static void setUiScalingFactorForAutomotiveForTesting(float scalingFactor) {
        sUiScalingFactorForAutomotiveForTesting = scalingFactor;
        ResettersForTesting.register(() -> sUiScalingFactorForAutomotiveForTesting = null);
    }

    public static void setCurrentSmallestScreenWidthForTesting(int smallestScreenWidth) {
        sSmallestScreenWidthForTesting = smallestScreenWidth;
        ResettersForTesting.register(() -> sSmallestScreenWidthForTesting = null);
    }

    public static void setIsOnDefaultDisplayForTesting(boolean value) {
        sIsOnDefaultDisplayForTesting = value;
        ResettersForTesting.register(() -> sIsOnDefaultDisplayForTesting = null);
    }

    /**
     * Returns the target scaling factor for automotive devices. The final effective scaling factor
     * is tweaked and may differ from this target. Note that this value can be overlaid by device
     * manufacturers.
     */
    public static float getTargetScalingFactorForAutomotive(Context context) {
        TypedValue automotiveUiScaleFactor = new TypedValue();
        context.getResources()
                .getValue(
                        org.chromium.ui.R.dimen.automotive_ui_scale_factor,
                        automotiveUiScaleFactor,
                        true);
        if (CommandLine.getInstance().hasSwitch(DisplaySwitches.CLAMP_AUTOMOTIVE_SCALE_UP)) {
            String maxAutomotiveScalingString =
                    CommandLine.getInstance()
                            .getSwitchValue(DisplaySwitches.CLAMP_AUTOMOTIVE_SCALE_UP);
            float maxAutomotiveScaling;
            try {
                maxAutomotiveScaling = Float.parseFloat(maxAutomotiveScalingString);
                if (maxAutomotiveScaling < automotiveUiScaleFactor.getFloat()) {
                    return maxAutomotiveScaling;
                }
            } catch (Exception ignored) {
            }
        }
        return automotiveUiScaleFactor.getFloat();
    }

    /**
     * Returns the UI density that has been adjusted for automotive displays. This density has been
     * adjusted by a scaling factor, which can be customized by device manufacturers, and rounded to
     * align with defined {@link DisplayMetrics} densities.
     */
    public static int getUiDensityForAutomotive(Context context, int baseDensity) {
        // Opt out of Clank's internal scaling if we have opted in to display compatibility for
        // CaRMA.
        if (doesDeviceHaveCarmaPhase1Version2Compliance(context) && isDisplayCompatApp(context)) {
            return baseDensity;
        }
        float uiScalingFactor =
                sUiScalingFactorForAutomotiveForTesting != null
                        ? sUiScalingFactorForAutomotiveForTesting
                        : getTargetScalingFactorForAutomotive(context);
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
        return Math.round(value / display.getDipScale());
    }

    /** Returns the given value converted from dp to px. */
    public static int dpToPx(DisplayAndroid display, int value) {
        return Math.round(value * display.getDipScale());
    }

    /**
     * Returns the display size in inches.
     *
     * @param display The display to get the size of.
     * @return The display size in inches.
     */
    public static double getDisplaySizeInInches(DisplayAndroid display) {
        double xInches = display.getDisplayWidth() / display.getXdpi();
        double yInches = display.getDisplayHeight() / display.getYdpi();
        return Math.sqrt(Math.pow(xInches, 2) + Math.pow(yInches, 2));
    }

    /**
     * Forces a {@link DisplayMetrics} object to a new density, scaling related DPI values.
     *
     * <p>This function modifies the passed-in {@link DisplayMetrics} object in-place.
     *
     * <p>This function updates {@link DisplayMetrics#density}, {@link DisplayMetrics#xdpi} and
     * {@link DisplayMetrics#ydpi}, but it doesn't update {@link DisplayMetrics#densityDpi} to
     * match. Use this only if you specifically require this partial update.
     *
     * @param density The new target density to set.
     * @param displayMetrics The DisplayMetrics object to modify in-place.
     */
    public static void forcedScaleUpDisplayMetrics(float density, DisplayMetrics displayMetrics) {
        final float scaling = density / displayMetrics.density;
        displayMetrics.density *= scaling;
        displayMetrics.xdpi *= scaling;
        displayMetrics.ydpi *= scaling;
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for automotive devices.
     *
     * <p>This function modifies the passed-in {@link DisplayMetrics} object in-place.
     *
     * @param displayMetrics The DisplayMetrics object to modify in-place.
     */
    public static void scaleUpDisplayMetricsForAutomotive(
            Context context, DisplayMetrics displayMetrics) {
        final int adjustedDensity = getUiDensityForAutomotive(context, displayMetrics.densityDpi);
        scaleUpDisplayMetrics(adjustedDensity, displayMetrics);
    }

    /**
     * Forces a {@link DisplayMetrics} object to a new densityDpi, scaling related DPI values.
     *
     * <p>This function modifies the passed-in {@link DisplayMetrics} object in-place.
     *
     * @param adjustedDensity The new target densityDpi to set.
     * @param displayMetrics The DisplayMetrics object to modify in-place.
     */
    private static void scaleUpDisplayMetrics(int adjustedDensity, DisplayMetrics displayMetrics) {
        final float scaling = (float) adjustedDensity / (float) displayMetrics.densityDpi;
        displayMetrics.density *= scaling;
        displayMetrics.densityDpi = adjustedDensity;
        displayMetrics.xdpi *= scaling;
        displayMetrics.ydpi *= scaling;
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
        if (sSmallestScreenWidthForTesting != null) {
            return sSmallestScreenWidthForTesting;
        }
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
        sUiScalingFactorForXrForTesting = scalingFactor;
        ResettersForTesting.register(() -> sUiScalingFactorForXrForTesting = null);
    }

    private static float getUiScalingFactorForXrFromResource(Context context) {
        TypedValue xrUiScaleFactor = new TypedValue();
        context.getResources()
                .getValue(org.chromium.ui.R.dimen.xr_ui_scale_factor, xrUiScaleFactor, true);
        return xrUiScaleFactor.getFloat();
    }

    /** Returns the scaling factor for the current device. */
    public static float getCurrentUiScalingFactor(Context context) {
        if (!isUiScaled()) return 1;
        if (DeviceInfo.isAutomotive()) {
            return sUiScalingFactorForAutomotiveForTesting != null
                    ? sUiScalingFactorForAutomotiveForTesting
                    : getTargetScalingFactorForAutomotive(context);
        }
        return sUiScalingFactorForXrForTesting != null
                ? sUiScalingFactorForXrForTesting
                : getUiScalingFactorForXrFromResource(context);
    }

    /** Get the density base on the UI scaling factor on XR devices. */
    public static int getUiDensityForXr(Context context, int baseDensity) {
        float uiScalingFactor =
                sUiScalingFactorForXrForTesting != null
                        ? sUiScalingFactorForXrForTesting
                        : getUiScalingFactorForXrFromResource(context);
        int rawScaledDensity = (int) (baseDensity * uiScalingFactor);
        // Round up to the nearest 10 to align with DisplayMetrics defined densities.
        return ((int) Math.ceil(rawScaledDensity / 10.0f)) * 10;
    }

    /**
     * Scales up the UI for the {@link DisplayMetrics} by the scaling factor for XR devices.
     *
     * <p>This function modifies the passed-in {@link DisplayMetrics} object in-place.
     *
     * @param context The context used to retrieve the scale value from {@link Resources}.
     * @param displayMetrics The DisplayMetrics object to modify in-place.
     */
    public static void scaleUpDisplayMetricsForXr(Context context, DisplayMetrics displayMetrics) {
        final int adjustedDensity = getUiDensityForXr(context, displayMetrics.densityDpi);

        scaleUpDisplayMetrics(adjustedDensity, displayMetrics);
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

    /**
     * Converts global dip coordinates (as in Web API spec) to local coordinates (display and pixel
     * coordinates relative to the origin of the display). Display is chosen by the most
     * intersection area. If none of the displays intersect with the given area null is returned.
     *
     * @param globalDipCoordinates Global coordinates in dip.
     * @return A pair of {@link DisplayAndroid} and local coordinates in pixels.
     */
    public static @Nullable Pair<DisplayAndroid, Rect> convertGlobalDipToLocalPxCoordinates(
            Rect globalDipCoordinates) {
        final DisplayAndroid display =
                DisplayAndroidManager.getInstance().getDisplayMatching(globalDipCoordinates);

        if (display == null) {
            return null;
        }

        final Rect displayGlobalDipBounds = display.getBounds();
        final Rect displayLocalPxBounds = display.getLocalBounds();
        final float displayDipScale = display.getDipScale();

        final RectF floatLocalCoordinatesPx =
                new RectF(
                        displayLocalPxBounds.left
                                + (globalDipCoordinates.left - displayGlobalDipBounds.left)
                                        * displayDipScale,
                        displayLocalPxBounds.top
                                + (globalDipCoordinates.top - displayGlobalDipBounds.top)
                                        * displayDipScale,
                        displayLocalPxBounds.right
                                + (globalDipCoordinates.right - displayGlobalDipBounds.right)
                                        * displayDipScale,
                        displayLocalPxBounds.bottom
                                + (globalDipCoordinates.bottom - displayGlobalDipBounds.bottom)
                                        * displayDipScale);

        final Rect localCoordinatesPx = new Rect();
        floatLocalCoordinatesPx.roundOut(localCoordinatesPx);

        return Pair.create(display, localCoordinatesPx);
    }

    /**
     * Converts local coordinates (display and pixel coordinates relative to the origin of the
     * display) to global dip coordinates (as in Web API spec). Rounds the resulting Rect outwards
     * to the nearest dip.
     *
     * @param display Reference display of the local coordinates provided.
     * @param localCoordinatesPx Display coordinates in pixels.
     * @return Global coordinates in dips.
     */
    public static Rect convertLocalPxToGlobalDipCoordinates(
            DisplayAndroid display, Rect localCoordinatesPx) {
        final float displayDipScale = display.getDipScale();
        final Rect displayBoundsGlobalCoordinatesDip = display.getBounds();

        final Rect localCoordinatesDip =
                scaleToEnclosingRect(localCoordinatesPx, 1.0f / displayDipScale);

        final Rect globalCoordinatesDip = new Rect(localCoordinatesDip);
        globalCoordinatesDip.offset(
                displayBoundsGlobalCoordinatesDip.left, displayBoundsGlobalCoordinatesDip.top);

        return globalCoordinatesDip;
    }

    /**
     * Scales a given rectangle by a specified factor and rounds the result to the smallest
     * integer-based rectangle that encloses it.
     *
     * @param rect The original {@link android.graphics.Rect} to be scaled.
     * @param scale The scaling factor.
     * @return The new {@link android.graphics.Rect} that encloses the scaled rectangle.
     */
    public static Rect scaleToEnclosingRect(Rect rect, float scale) {
        final RectF scaledRect =
                new RectF(
                        rect.left * scale,
                        rect.top * scale,
                        rect.right * scale,
                        rect.bottom * scale);

        final Rect enclosingRect = new Rect();
        scaledRect.roundOut(enclosingRect);

        return enclosingRect;
    }

    /**
     * Determine whether the given context is associated with the default display.
     *
     * @param context The context to determine display state.
     * @return {@code true} if the context is associated with the default display, {@code false}
     *     otherwise.
     */
    public static boolean isContextInDefaultDisplay(Context context) {
        if (sIsOnDefaultDisplayForTesting != null) {
            return sIsOnDefaultDisplayForTesting;
        }
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        return display.getDisplayId() == Display.DEFAULT_DISPLAY;
    }

    /**
     * Checks if the device OS supports CaRMA Phase 1 version 2.
     *
     * @param context The context used to access the PackageManager.
     * @return {@code true} if the device supports the required feature phase and version, {@code
     *     false} otherwise.
     */
    public static boolean doesDeviceHaveCarmaPhase1Version2Compliance(Context context) {
        if (sCarmaPhase1Version2ComplianceForTesting != null) {
            return sCarmaPhase1Version2ComplianceForTesting;
        }
        return context.getPackageManager().hasSystemFeature(CARMA_PHASE_1_COMPLIANCE, 2);
    }

    /** Checks if the app has opted in to Display Compatibility via its manifest metadata. */
    public static boolean isDisplayCompatApp(Context context) {
        if (sIsDisplayCompatAppForTesting != null) {
            return sIsDisplayCompatAppForTesting;
        }

        try {
            ApplicationInfo applicationInfo =
                    context.getPackageManager()
                            .getApplicationInfo(
                                    context.getPackageName(), PackageManager.GET_META_DATA);

            if (applicationInfo.metaData == null) {
                return false;
            }
            return applicationInfo.metaData.getBoolean(CARMA_DISPLAY_COMPAT_APP_META_DATA);

        } catch (NameNotFoundException e) {
            return false;
        }
    }

    /**
     * Adjusts {@code inputRect} to fit inside {@code limitingRect}.
     *
     * <p>If {@code inputRect} fits fully inside {@code limitingRect}, this method returns a copy of
     * {@code inputRect}.
     *
     * <p>Otherwise, the returned {@link Rect} will be a copy of {@code inputRect} modified so that
     * it is fully inside {@code limitingRect} and is the closest match to {@code inputRect},
     * prioritising preserving original width and height first, then minimizing the Manhattan
     * distance between {@code inputRect} and the adjusted one.
     *
     * <p>If {@code inputRect} is longer than {@code limitingRect} in precisely one axis, the
     * displacement alongside the other axis will be minimised between {@code inputRect} and the
     * adjusted one.
     *
     * <p>If {@code inputRect} is longer than {@code limitingRect} in both axes, {@code
     * limitingRect} will be returned.
     *
     * @param inputRect The {@link Rect} to adjust.
     * @param limitingRect The {@link Rect} that defines the bounds.
     * @return A new {@link Rect}, guaranteed to be fully within {@code limitingRect}.
     */
    @SuppressWarnings("CheckResult")
    public static Rect clampRect(Rect inputRect, Rect limitingRect) {
        Rect output = new Rect(inputRect);

        output.offset(Math.max(limitingRect.left - output.left, 0), 0);
        output.offset(Math.min(limitingRect.right - output.right, 0), 0);
        output.offset(0, Math.max(limitingRect.top - output.top, 0));
        output.offset(0, Math.min(limitingRect.bottom - output.bottom, 0));

        output.intersect(limitingRect);

        return output;
    }

    /**
     * Adjusts the given bounds to fit the given display.
     *
     * <p>Please see {@link #clampRect(Rect, Rect)} for how the bounds are adjusted.
     *
     * @param boundsPx The rectangle to adjust, in pixels. Its coordinates should be relative to the
     *     display, with (0, 0) at the top-left corner and positive axes going rightward and
     *     downward.
     * @param display The display that defines the containing bounds.
     * @return A new Rect, guaranteed to be fully within the display bounds. Uses the same
     *     coordinate system as the initial Rect.
     */
    public static Rect clampWindowToDisplay(Rect boundsPx, DisplayAndroid display) {
        return clampRect(boundsPx, display.getLocalBounds());
    }

    public static void setCarmaPhase1Version2ComplianceForTesting(
            boolean carmaPhase1Version2ComplianceForTesting) {
        sCarmaPhase1Version2ComplianceForTesting = carmaPhase1Version2ComplianceForTesting;
        ResettersForTesting.register(() -> sCarmaPhase1Version2ComplianceForTesting = null);
    }

    public static void setIsDisplayCompatAppForTesting(boolean isDisplayCompatAppForTesting) {
        sIsDisplayCompatAppForTesting = isDisplayCompatAppForTesting;
        ResettersForTesting.register(() -> sIsDisplayCompatAppForTesting = null);
    }
}

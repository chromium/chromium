// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.hardware.display.DeviceProductInfo;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;
import androidx.core.os.BuildCompat;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/** A DisplayAndroid implementation tied to a physical Display. */
@NullMarked
/* package */ class PhysicalDisplayAndroid extends DisplayAndroid {
    private static final String TAG = "DisplayAndroid";

    // The behavior of observing window configuration changes using ComponentCallbacks is new in S.
    private static final boolean USE_CONFIGURATION = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S;

    // When this object exists, a positive value means that the forced DIP scale is set and
    // the zero means it is not. The non existing object (i.e. null reference) means that
    // the existence and value of the forced DIP scale has not yet been determined.
    private static @Nullable Float sForcedDIPScale;

    private static @Nullable Float getHdrSdrRatio(Display display) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return null;
        return display.getHdrSdrRatio();
    }

    private static boolean isHdr(Display display) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return false;
        return display.isHdr() && display.isHdrSdrRatioAvailable();
    }

    private static boolean hasForcedDIPScale() {
        return getForcedDIPScale() > 0;
    }

    private static float getForcedDIPScale() {
        if (sForcedDIPScale == null) {
            float value = 0.0f;
            String forcedScaleAsString =
                    CommandLine.getInstance()
                            .getSwitchValue(DisplaySwitches.FORCE_DEVICE_SCALE_FACTOR);
            if (forcedScaleAsString != null) {
                try {
                    value = Float.valueOf(forcedScaleAsString);
                } catch (NumberFormatException e) {
                }

                if (value <= 0.0f) {
                    // Strings that do not represent numbers are discarded.
                    Log.w(TAG, "Ignoring invalid forced DIP scale: %s", forcedScaleAsString);
                    value = 0.0f;
                }
            }
            sForcedDIPScale = value;
        }
        return sForcedDIPScale.floatValue();
    }

    /* package */ static boolean isForcedDIPScaleChanged() {
        final float forcedDIPScale = getForcedDIPScale();
        sForcedDIPScale = null;

        return !MathUtils.areFloatsEqual(getForcedDIPScale(), forcedDIPScale);
    }

    /**
     * This method returns the bitsPerPixel without the alpha channel, as this is the value expected
     * by Chrome and the CSS media queries.
     */
    @SuppressWarnings("deprecation")
    private static int bitsPerPixel(int pixelFormatId) {
        // For JB-MR1 and above, this is the only value, so we can hard-code the result.
        if (pixelFormatId == PixelFormat.RGBA_8888) return 24;

        PixelFormat pixelFormat = new PixelFormat();
        PixelFormat.getPixelFormatInfo(pixelFormatId, pixelFormat);
        if (!PixelFormat.formatHasAlpha(pixelFormatId)) return pixelFormat.bitsPerPixel;

        switch (pixelFormatId) {
            case PixelFormat.RGBA_1010102:
                return 30;

            case PixelFormat.RGBA_4444:
                return 12;

            case PixelFormat.RGBA_5551:
                return 15;

            case PixelFormat.RGBA_8888:
                assert false;
                // fall through RGBX_8888 does not have an alpha channel even if it has 8 reserved
                // bits at the end.
            case PixelFormat.RGBX_8888:
            case PixelFormat.RGB_888:
            default:
                return 24;
        }
    }

    @SuppressWarnings("deprecation")
    private static int bitsPerComponent(int pixelFormatId) {
        switch (pixelFormatId) {
            case PixelFormat.RGBA_4444:
                return 4;

            case PixelFormat.RGBA_5551:
                return 5;

            case PixelFormat.RGBA_8888:
            case PixelFormat.RGBX_8888:
            case PixelFormat.RGB_888:
                return 8;

            case PixelFormat.RGB_332:
                return 2;

            case PixelFormat.RGB_565:
                return 5;

                // Non-RGB formats.
            case PixelFormat.A_8:
            case PixelFormat.LA_88:
            case PixelFormat.L_8:
                return 0;

                // Unknown format. Use 8 as a sensible default.
            default:
                return 8;
        }
    }

    private final @Nullable Context mWindowContext;
    private final @Nullable WindowManager mWindowManager;
    private final @Nullable ComponentCallbacks mComponentCallbacks;
    private final Display mDisplay;
    private @Nullable RectF mDisplayAbsoluteCoordinates;
    private @Nullable Consumer<Display> mHdrSdrRatioCallback;

    /* package */ PhysicalDisplayAndroid(
            Display display,
            @Nullable RectF displayAbsoluteCoordinates,
            boolean disableHdrSdkRatioCallback) {
        super(display.getDisplayId());
        if (USE_CONFIGURATION) {
            Context appContext = ContextUtils.getApplicationContext();
            // `createWindowContext` on some devices writes to disk. See crbug.com/1408587.
            try (@SuppressWarnings("unused")
                    StrictModeContext strictModeContext =
                            StrictModeContext.allowAllThreadPolicies()) {
                mWindowContext =
                        appContext.createWindowContext(
                                display, WindowManager.LayoutParams.TYPE_APPLICATION, null);
            }
            assert display.getDisplayId() == mWindowContext.getDisplay().getDisplayId();
            mComponentCallbacks =
                    new ComponentCallbacks() {
                        @Override
                        public void onLowMemory() {}

                        @Override
                        public void onConfigurationChanged(Configuration newConfig) {
                            updateFromConfiguration();
                        }
                    };
            mWindowContext.registerComponentCallbacks(mComponentCallbacks);
            mWindowManager = mWindowContext.getSystemService(WindowManager.class);
            mDisplay = mWindowContext.getDisplay();
            mDisplayAbsoluteCoordinates = displayAbsoluteCoordinates;
            updateFromConfiguration();
        } else {
            mWindowContext = null;
            mWindowManager = null;
            mComponentCallbacks = null;
            mDisplay = display;
            updateFromDisplay(display);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                && mDisplay.isHdrSdrRatioAvailable()
                && !disableHdrSdkRatioCallback) {
            mHdrSdrRatioCallback = this::hdrSdrRatioChanged;
            mDisplay.registerHdrSdrRatioChangedListener(
                    (Runnable runnable) -> {
                        ThreadUtils.getUiThreadHandler().post(runnable);
                    },
                    mHdrSdrRatioCallback);
        } else {
            mHdrSdrRatioCallback = null;
        }
    }

    @Override
    public @Nullable Context getWindowContext() {
        return mWindowContext;
    }

    @RequiresApi(VERSION_CODES.R)
    /* package */ void updateBounds(RectF displayAbsoluteCoordinates) {
        mDisplayAbsoluteCoordinates = displayAbsoluteCoordinates;
        updateFromConfiguration();
    }

    @RequiresApi(VERSION_CODES.R)
    private void updateFromConfiguration() {
        assumeNonNull(mWindowContext);
        assumeNonNull(mWindowManager);

        final DisplayMetrics displayMetrics = mWindowContext.getResources().getDisplayMetrics();
        final float initialDensity = displayMetrics.density;

        if (hasForcedDIPScale()) {
            DisplayUtil.forcedScaleUpDisplayMetrics(getForcedDIPScale(), displayMetrics);
        } else if (DeviceInfo.isAutomotive()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED)) {
            mDisplay.getRealMetrics(displayMetrics);
            DisplayUtil.scaleUpDisplayMetricsForAutomotive(mWindowContext, displayMetrics);
        } else if (DeviceInfo.isXr()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.XR_WEB_UI_SCALE_UP_ENABLED)) {
            mDisplay.getRealMetrics(displayMetrics);
            DisplayUtil.scaleUpDisplayMetricsForXr(mWindowContext, displayMetrics);
        }

        final Insets insets =
                mWindowManager
                        .getCurrentWindowMetrics()
                        .getWindowInsets()
                        .getInsetsIgnoringVisibility(
                                WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());

        final Rect boundsInPixels = mWindowManager.getMaximumWindowMetrics().getBounds();

        Rect bounds;
        Rect workArea;
        if (mDisplayAbsoluteCoordinates != null) {
            final float scalingFactor = initialDensity / displayMetrics.density;
            final RectF scaledAbsoluteBounds =
                    new RectF(
                            mDisplayAbsoluteCoordinates.left * scalingFactor,
                            mDisplayAbsoluteCoordinates.top * scalingFactor,
                            mDisplayAbsoluteCoordinates.right * scalingFactor,
                            mDisplayAbsoluteCoordinates.bottom * scalingFactor);
            bounds = new Rect();
            scaledAbsoluteBounds.roundOut(bounds);

            final RectF workAreaAbsoluteCoordinates =
                    new RectF(
                            scaledAbsoluteBounds.left + insets.left / displayMetrics.density,
                            scaledAbsoluteBounds.top + insets.top / displayMetrics.density,
                            scaledAbsoluteBounds.right - insets.right / displayMetrics.density,
                            scaledAbsoluteBounds.bottom - insets.bottom / displayMetrics.density);
            workArea = new Rect();
            workAreaAbsoluteCoordinates.roundOut(workArea);
        } else {
            bounds =
                    DisplayUtil.scaleToEnclosingRect(boundsInPixels, 1.0f / displayMetrics.density);
            workArea =
                    DisplayUtil.scaleToEnclosingRect(
                            new Rect(
                                    boundsInPixels.left + insets.left,
                                    boundsInPixels.top + insets.top,
                                    boundsInPixels.right - insets.right,
                                    boundsInPixels.bottom - insets.bottom),
                            1.0f / displayMetrics.density);
        }

        updateCommon(
                bounds,
                workArea,
                boundsInPixels.width(),
                boundsInPixels.height(),
                displayMetrics.density,
                displayMetrics.xdpi,
                displayMetrics.ydpi,
                mWindowContext.getDisplay());
    }

    /* package */ void onDisplayRemoved() {
        if (USE_CONFIGURATION) {
            assumeNonNull(mWindowContext);
            assumeNonNull(mComponentCallbacks);
            mWindowContext.unregisterComponentCallbacks(mComponentCallbacks);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                && mHdrSdrRatioCallback != null) {
            mDisplay.unregisterHdrSdrRatioChangedListener(mHdrSdrRatioCallback);
            mHdrSdrRatioCallback = null;
        }
    }

    @SuppressWarnings("deprecation")
    /* package */ void updateFromDisplay(Display display) {
        if (USE_CONFIGURATION) {
            assert display.getDisplayId() == mDisplay.getDisplayId();
            // Needed to update non-configuration info such as refresh rate.
            updateFromConfiguration();
            return;
        }

        Point size = new Point();
        DisplayMetrics displayMetrics = new DisplayMetrics();
        display.getRealSize(size);
        display.getRealMetrics(displayMetrics);

        if (hasForcedDIPScale()) {
            DisplayUtil.forcedScaleUpDisplayMetrics(getForcedDIPScale(), displayMetrics);
        } else if (DeviceInfo.isAutomotive()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED)) {
            DisplayUtil.scaleUpDisplayMetricsForAutomotive(
                    ContextUtils.getApplicationContext(), displayMetrics);
        } else if (DeviceInfo.isXr()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.XR_WEB_UI_SCALE_UP_ENABLED)) {
            DisplayUtil.scaleUpDisplayMetricsForXr(
                    ContextUtils.getApplicationContext(), displayMetrics);
        }

        Rect bounds =
                DisplayUtil.scaleToEnclosingRect(
                        new Rect(0, 0, size.x, size.y), 1.0f / displayMetrics.density);
        Rect workArea = new Rect(bounds);

        updateCommon(
                bounds,
                workArea,
                size.x,
                size.y,
                displayMetrics.density,
                displayMetrics.xdpi,
                displayMetrics.ydpi,
                display);
    }

    private void hdrSdrRatioChanged(Display display) {
        assert display.getDisplayId() == mDisplay.getDisplayId();
        super.update(
                /* name= */ null,
                /* bounds= */ null,
                /* workArea= */ null,
                /* width= */ null,
                /* height= */ null,
                /* dipScale= */ null,
                /* xdpi= */ null,
                /* ydpi= */ null,
                /* bitsPerPixel= */ null,
                /* bitsPerComponent= */ null,
                /* rotation= */ null,
                /* isDisplayWideColorGamut= */ null,
                /* isDisplayServerWideColorGamut= */ null,
                /* refreshRate= */ null,
                /* currentMode= */ null,
                /* supportedModes= */ null,
                isHdr(mDisplay),
                getHdrSdrRatio(mDisplay),
                /* isInternal= */ null,
                /* arrInfo= */ null);
    }

    private void updateCommon(
            Rect bounds,
            Rect workArea,
            int width,
            int height,
            float density,
            float xdpi,
            float ydpi,
            Display display) {
        boolean isWideColorGamut = display.isWideColorGamut();

        int pixelFormatId = PixelFormat.RGBA_8888;

        // Note: getMode() and getSupportedModes() can return null in some situations - see
        // crbug.com/1401322.
        // Can also throw when modeId=-1 (b/441513616).
        Display.Mode currentMode = null;
        try {
            currentMode = display.getMode();
        } catch (Exception e) {
            Log.w(TAG, "Invalid display mode", e);
        }
        Display.Mode[] modes = display.getSupportedModes();
        List<Display.Mode> supportedModes = null;
        if (modes != null && modes.length > 0) {
            supportedModes = Arrays.asList(modes);
        }

        boolean isInternal = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            DeviceProductInfo deviceProductInfo = display.getDeviceProductInfo();
            if (deviceProductInfo != null) {
                isInternal =
                        deviceProductInfo.getConnectionToSinkType()
                                == DeviceProductInfo.CONNECTION_TO_SINK_BUILT_IN;
            }
        }

        AdaptiveRefreshRateInfo arrInfo = null;
        if (BuildCompat.isAtLeastB()) {
            boolean hasArrSupport = display.hasArrSupport();
            float suggestedFrameRateHigh = 0.0f;
            if (hasArrSupport) {
                suggestedFrameRateHigh =
                        display.getSuggestedFrameRate(Display.FRAME_RATE_CATEGORY_HIGH);
            }
            arrInfo = new AdaptiveRefreshRateInfo(hasArrSupport, suggestedFrameRateHigh);
        }

        super.update(
                display.getName(),
                bounds,
                workArea,
                width,
                height,
                density,
                xdpi,
                ydpi,
                bitsPerPixel(pixelFormatId),
                bitsPerComponent(pixelFormatId),
                display.getRotation(),
                isWideColorGamut,
                null,
                display.getRefreshRate(),
                currentMode,
                supportedModes,
                isHdr(display),
                getHdrSdrRatio(display),
                isInternal,
                arrInfo);
    }

    public static void setHasForcedDIPScaleForTesting(float forcedDIPScale) {
        sForcedDIPScale = forcedDIPScale;
        ResettersForTesting.register(() -> sForcedDIPScale = null);
    }
}

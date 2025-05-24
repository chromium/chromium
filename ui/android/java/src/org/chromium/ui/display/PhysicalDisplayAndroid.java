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
import android.hardware.display.DeviceProductInfo;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;
import androidx.core.os.BuildCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.XrUtils;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/** A DisplayAndroid implementation tied to a physical Display. */
@NullMarked
/* package */ class PhysicalDisplayAndroid extends DisplayAndroid {
    private static final String TAG = "DisplayAndroid";

    // The behavior of observing window configuration changes using ComponentCallbacks is new in S.
    private static final boolean USE_CONFIGURATION = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S;

    // Insets that define the area where content can't be displayed.
    protected static final int WINDOW_INSETS_TYPE =
            WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout();

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

    @EnsuresNonNull("sForcedDIPScale")
    private static boolean hasForcedDIPScale() {
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
        return sForcedDIPScale.floatValue() > 0;
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
    private @Nullable Consumer<Display> mHdrSdrRatioCallback;

    /* package */ PhysicalDisplayAndroid(Display display, boolean disableHdrSdkRatioCallback) {
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
            updateFromConfiguration();
        } else {
            mWindowContext = null;
            mWindowManager = null;
            mComponentCallbacks = null;
            mDisplay = display;
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
    private Insets getWindowInsets() {
        return assumeNonNull(mWindowManager)
                .getCurrentWindowMetrics()
                .getWindowInsets()
                .getInsetsIgnoringVisibility(WINDOW_INSETS_TYPE);
    }

    @RequiresApi(VERSION_CODES.R)
    private void updateFromConfiguration() {
        assumeNonNull(mWindowContext);
        assumeNonNull(mWindowManager);

        Rect bounds = mWindowManager.getMaximumWindowMetrics().getBounds();
        Insets insets = getWindowInsets();

        DisplayMetrics displayMetrics = mWindowContext.getResources().getDisplayMetrics();

        if (DeviceInfo.isAutomotive()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED)) {
            mDisplay.getRealMetrics(displayMetrics);
            DisplayUtil.scaleUpDisplayMetricsForAutomotive(mWindowContext, displayMetrics);
        } else if (XrUtils.isXrDevice()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.XR_WEB_UI_SCALE_UP_ENABLED)) {
            mDisplay.getRealMetrics(displayMetrics);
            DisplayUtil.scaleUpDisplayMetricsForXr(mWindowContext, displayMetrics);
        }

        updateCommon(
                bounds,
                insets,
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

        if (DeviceInfo.isAutomotive()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED)) {
            DisplayUtil.scaleUpDisplayMetricsForAutomotive(
                    ContextUtils.getApplicationContext(), displayMetrics);
        } else if (XrUtils.isXrDevice()
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.XR_WEB_UI_SCALE_UP_ENABLED)) {
            DisplayUtil.scaleUpDisplayMetricsForXr(
                    ContextUtils.getApplicationContext(), displayMetrics);
        }

        updateCommon(
                new Rect(0, 0, size.x, size.y),
                null,
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
                /* insets= */ null,
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
            @Nullable Insets insets,
            float density,
            float xdpi,
            float ydpi,
            Display display) {
        if (hasForcedDIPScale()) density = sForcedDIPScale.floatValue();
        boolean isWideColorGamut = false;
        // Although this API was added in Android O, it was buggy.
        // Restrict to Android Q, where it was fixed.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            isWideColorGamut = display.isWideColorGamut();
        }

        int pixelFormatId = PixelFormat.RGBA_8888;

        // Note: getMode() and getSupportedModes() can return null in some situations - see
        // crbug.com/1401322.
        Display.Mode currentMode = display.getMode();
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
                insets,
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
}

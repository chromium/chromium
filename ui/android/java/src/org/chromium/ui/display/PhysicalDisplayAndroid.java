// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;

import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

/** A DisplayAndroid implementation tied to a physical Display. */
/* package */ class PhysicalDisplayAndroid extends DisplayAndroid {
    private static final String TAG = "DisplayAndroid";

    // The behavior of observing window configuration changes using ComponentCallbacks is new in S.
    private static final boolean USE_CONFIGURATION = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S;

    // When this object exists, a positive value means that the forced DIP scale is set and
    // the zero means it is not. The non existing object (i.e. null reference) means that
    // the existence and value of the forced DIP scale has not yet been determined.
    private static Float sForcedDIPScale;

    private static Float getHdrSdrRatio(Display display) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return null;
        return display.getHdrSdrRatio();
    }

    private static boolean isHdr(Display display) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return false;
        return display.isHdr() && display.isHdrSdrRatioAvailable();
    }

    private static boolean hasForcedDIPScale() {
        if (sForcedDIPScale == null) {
            String forcedScaleAsString =
                    CommandLine.getInstance()
                            .getSwitchValue(DisplaySwitches.FORCE_DEVICE_SCALE_FACTOR);
            if (forcedScaleAsString == null) {
                sForcedDIPScale = Float.valueOf(0.0f);
            } else {
                boolean isInvalid = false;
                try {
                    sForcedDIPScale = Float.valueOf(forcedScaleAsString);
                    // Negative values are discarded.
                    if (sForcedDIPScale.floatValue() <= 0.0f) isInvalid = true;
                } catch (NumberFormatException e) {
                    // Strings that do not represent numbers are discarded.
                    isInvalid = true;
                }

                if (isInvalid) {
                    Log.w(TAG, "Ignoring invalid forced DIP scale '" + forcedScaleAsString + "'");
                    sForcedDIPScale = Float.valueOf(0.0f);
                }
            }
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

    private final Context mWindowContext;
    private final ComponentCallbacks mComponentCallbacks;
    private final Display mDisplay;
    private Consumer<Display> mHdrSdrRatioCallback;

    /* package */ PhysicalDisplayAndroid(Display display, boolean disableHdrSdkRatioCallback) {
        super(display.getDisplayId());
        if (USE_CONFIGURATION) {
            Context appContext = ContextUtils.getApplicationContext();
            // `createWindowContext` on some devices writes to disk. See crbug.com/1408587.
            try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
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
            mDisplay = mWindowContext.getDisplay();
            updateFromConfiguration();
        } else {
            mWindowContext = null;
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
    public Context getWindowContext() {
        return mWindowContext;
    }

    @RequiresApi(api = VERSION_CODES.R)
    private void updateFromConfiguration() {
        Point size = new Point();
        WindowManager windowManager = mWindowContext.getSystemService(WindowManager.class);
        Rect rect = windowManager.getMaximumWindowMetrics().getBounds();
        size.set(rect.width(), rect.height());
        DisplayMetrics displayMetrics = mWindowContext.getResources().getDisplayMetrics();

        if (BuildInfo.getInstance().isAutomotive
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED)) {
            mDisplay.getRealMetrics(displayMetrics);
            DisplayUtil.scaleUpDisplayMetricsForAutomotive(mWindowContext, displayMetrics);
        }
        updateCommon(
                size,
                displayMetrics.density,
                displayMetrics.xdpi,
                displayMetrics.ydpi,
                mWindowContext.getDisplay());
    }

    /* package */ void onDisplayRemoved() {
        if (USE_CONFIGURATION) {
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

        if (BuildInfo.getInstance().isAutomotive
                && CommandLine.getInstance()
                        .hasSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED)) {
            DisplayUtil.scaleUpDisplayMetricsForAutomotive(
                    ContextUtils.getApplicationContext(), displayMetrics);
        }
        updateCommon(
                size, displayMetrics.density, displayMetrics.xdpi, displayMetrics.ydpi, display);
    }

    private void hdrSdrRatioChanged(Display display) {
        assert display.getDisplayId() == mDisplay.getDisplayId();
        super.update(
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                isHdr(mDisplay),
                getHdrSdrRatio(mDisplay));
    }

    private void updateCommon(Point size, float density, float xdpi, float ydpi, Display display) {
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

        super.update(
                size,
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
                getHdrSdrRatio(display));
    }
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.annotation.TargetApi;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForO;

import java.util.Arrays;
import java.util.List;

/**
 * A DisplayAndroid implementation tied to a physical Display.
 */
/* package */ class PhysicalDisplayAndroid extends DisplayAndroid {
    private static final String TAG = "DisplayAndroid";

    // When this object exists, a positive value means that the forced DIP scale is set and
    // the zero means it is not. The non existing object (i.e. null reference) means that
    // the existence and value of the forced DIP scale has not yet been determined.
    private static Float sForcedDIPScale;

    private static boolean hasForcedDIPScale() {
        if (sForcedDIPScale == null) {
            String forcedScaleAsString = CommandLine.getInstance().getSwitchValue(
                    DisplaySwitches.FORCE_DEVICE_SCALE_FACTOR);
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
            // fall through
            // RGBX_8888 does not have an alpha channel even if it has 8 reserved bits at the end.
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

    /* package */ PhysicalDisplayAndroid(Display display) {
        super(display.getDisplayId());
    }

    @SuppressWarnings("deprecation")
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    /* package */ void updateFromDisplay(Display display) {
        Point size = new Point();
        DisplayMetrics displayMetrics = new DisplayMetrics();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            display.getRealSize(size);
            display.getRealMetrics(displayMetrics);
        } else {
            display.getSize(size);
            display.getMetrics(displayMetrics);
        }
        if (hasForcedDIPScale()) displayMetrics.density = sForcedDIPScale.floatValue();
        boolean isWideColorGamut = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            isWideColorGamut = ApiHelperForO.isWideColorGamut(display);
        }

        // JellyBean MR1 and later always uses RGBA_8888.
        int pixelFormatId = (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1)
                ? display.getPixelFormat()
                : PixelFormat.RGBA_8888;

        Display.Mode currentMode = null;
        List<Display.Mode> supportedModes = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            currentMode = display.getMode();
            supportedModes = Arrays.asList(display.getSupportedModes());
            assert currentMode != null;
            assert supportedModes != null;
            assert supportedModes.size() > 0;
        }

        super.update(size, displayMetrics.density, bitsPerPixel(pixelFormatId),
                bitsPerComponent(pixelFormatId), display.getRotation(), isWideColorGamut, null,
                display.getRefreshRate(), currentMode, supportedModes);
    }
}

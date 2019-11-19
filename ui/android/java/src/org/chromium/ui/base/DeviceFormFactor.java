// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;

import androidx.annotation.UiThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.ui.R;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/**
 * UI utilities for accessing form factor information.
 */
public class DeviceFormFactor {
    /**
     * Miniumum screen size in dp to be considered a tablet. Matches the value
     * used by res/ directories. E.g.: res/values-sw600dp/values.xml
     */
    public static final int MINIMUM_TABLET_WIDTH_DP = 600;

    /**
     * Matches the value set in res/values-sw600dp/values.xml
     */
    private static final int SCREEN_BUCKET_TABLET = 2;

    /**
     * Matches the value set in res/values-sw720dp/values.xml
     */
    private static final int SCREEN_BUCKET_LARGET_TABLET = 3;

    /**
     * Each activity could be on a different display, and this will just tell you whether the
     * display associated with the application context is "tablet sized".
     * Use {@link #isNonMultiDisplayContextOnTablet} or {@link #isWindowOnTablet} instead.
     */
    @CalledByNative
    @Deprecated
    public static boolean isTablet() {
        return detectScreenWidthBucket(ContextUtils.getApplicationContext())
                >= SCREEN_BUCKET_TABLET;
    }

    /**
     * See {@link DisplayAndroid#getNonMultiDisplay}} for what "NonMultiDisplay" means.
     * When possible, it is generally more correct to use {@link #isWindowOnTablet}.
     * Only Activity instances and Contexts that wrap Activities are meaningfully associated with
     * displays, so care should be taken to pass a context that makes sense.
     *
     * @return Whether the display associated with the given context is large enough to be
     *         considered a tablet and will thus load tablet-specific resources (those in the config
     *         -sw600).
     *         Not affected by Android N multi-window, but can change for external displays.
     *         E.g. http://developer.samsung.com/samsung-dex/testing
     */
    public static boolean isNonMultiDisplayContextOnTablet(Context context) {
        return detectScreenWidthBucket(context) >= SCREEN_BUCKET_TABLET;
    }

    /**
     * @return Whether the display associated with the window is large enough to be
     *         considered a tablet and will thus load tablet-specific resources (those in the config
     *         -sw600).
     *         Not affected by Android N multi-window, but can change for external displays.
     *         E.g. http://developer.samsung.com/samsung-dex/testing
     */
    @UiThread
    public static boolean isWindowOnTablet(WindowAndroid windowAndroid) {
        return detectScreenWidthBucket(windowAndroid) >= SCREEN_BUCKET_TABLET;
    }

    /**
     * @return Whether the display associated with the given context is large enough to be
     *         considered a large tablet and will thus load large-tablet-specific resources (those
     *         in the config -sw720).
     *         Not affected by Android N multi-window, but can change for external displays.
     *         E.g. http://developer.samsung.com/samsung-dex/testing
     */
    public static boolean isNonMultiDisplayContextOnLargeTablet(Context context) {
        return detectScreenWidthBucket(context) == SCREEN_BUCKET_LARGET_TABLET;
    }

    /**
     * Detect the screen width bucket by loading the min_screen_width_bucket value (Android will
     * select the value from the correct directory; values, *-sw600dp, *-sw720dp). We can't use any
     * shortcuts here since there are several devices that are phone or tablet, but load each
     * others' resources (see https://crbug.com/850096 and https://crbug.com/669974 for more info).
     * @param context An Android context to read resources from.
     * @return The screen width bucket the device is in (see constants at the top of this class).
     */
    private static int detectScreenWidthBucket(Context context) {
        return context.getResources().getInteger(R.integer.min_screen_width_bucket);
    }

    private static int detectScreenWidthBucket(WindowAndroid windowAndroid) {
        ThreadUtils.assertOnUiThread();
        Context context = windowAndroid.getContext().get();
        if (context == null) return 0;
        return context.getResources().getInteger(R.integer.min_screen_width_bucket);
    }

    /**
     * @return The minimum width in px at which the display should be treated like a tablet for
     *         layout.
     */
    @UiThread
    public static int getNonMultiDisplayMinimumTabletWidthPx(Context context) {
        return getMinimumTabletWidthPx(DisplayAndroid.getNonMultiDisplay(context));
    }

    /**
     * @return The minimum width in px at which the display should be treated like a tablet for
     *         layout.
     */
    public static int getMinimumTabletWidthPx(DisplayAndroid display) {
        return DisplayUtil.dpToPx(display, DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP);
    }
}

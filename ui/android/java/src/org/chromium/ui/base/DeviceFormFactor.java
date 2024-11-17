// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;

import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.ui.R;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/** UI utilities for accessing form factor information. */
public class DeviceFormFactor {
    /**
     * Phone form factor.
     *
     * <p>Identified by <code>isNonMultiDisplayContextOnTablet() == false</code>.
     */
    public static final String PHONE = "Phone";

    /**
     * Tablet form factor, including {@code #LARGETABLET} below.
     *
     * <p>Identified by <code>isNonMultiDisplayContextOnTablet() == true</code>.
     */
    public static final String TABLET = "Tablet";

    /**
     * Minimum screen size in dp to be considered a tablet. Matches the value used by res/
     * directories. E.g.: res/values-sw600dp/values.xml
     */
    public static final int MINIMUM_TABLET_WIDTH_DP = 600;

    /** Matches the value set in res/values-sw600dp/values.xml */
    @VisibleForTesting public static final int SCREEN_BUCKET_TABLET = 2;

    /** Matches the value set in res/values-sw720dp/values.xml */
    private static final int SCREEN_BUCKET_LARGET_TABLET = 3;

    /** See {@link #setIsTabletForTesting(boolean)}. */
    private static Boolean sIsTabletForTesting;

    /**
     * Each activity could be on a different display, and this will just tell you whether the
     * display associated with the application context is "tablet sized". Use {@link
     * #isNonMultiDisplayContextOnTablet} or {@link #isWindowOnTablet} instead.
     */
    @CalledByNative
    @Deprecated
    public static boolean isTablet() {
        if (sIsTabletForTesting != null) {
            return sIsTabletForTesting;
        }
        return detectScreenWidthBucket(ContextUtils.getApplicationContext())
                >= SCREEN_BUCKET_TABLET;
    }

    /**
     * Modifies the output of {@link #isTablet()} for testing. Note that it is preferable to use
     * {@link org.robolectric.annotation.Config} annotations to specify screen dimensions when
     * possible. This method exists for instances where it is not possible or where it is cumbersome
     * to do so, e.g. when device form factor is parameterized in a test suite.
     */
    public static void setIsTabletForTesting(Boolean isTablet) {
        sIsTabletForTesting = isTablet;
        ResettersForTesting.register(() -> sIsTabletForTesting = null);
    }

    /**
     * See {@link DisplayAndroid#getNonMultiDisplay}} for what "NonMultiDisplay" means. When
     * possible, it is generally more correct to use {@link #isWindowOnTablet}. Only Activity
     * instances and Contexts that wrap Activities are meaningfully associated with displays, so
     * care should be taken to pass a context that makes sense.
     *
     * @return Whether the display associated with the given context is large enough to be
     *     considered a tablet and will thus load tablet-specific resources (those in the config
     *     -sw600). Not affected by Android N multi-window, but can change for external displays.
     *     E.g. http://developer.samsung.com/samsung-dex/testing
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

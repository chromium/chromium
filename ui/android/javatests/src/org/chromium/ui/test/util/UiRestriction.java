// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.pm.PackageManager;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.build.BuildConfig;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * UiRestrictions list form factor restrictions, that are usable with the {@link Restriction}
 * annotation in layers depending on //ui. E.g. <code>
 *   \@Restriction({DeviceFormFactor.RESTRICTION_TYPE_PHONE})
 * </code>
 */
public final class UiRestriction {
    private static Boolean sIsDesktop;
    private static Boolean sIsDesktopFreeform;
    private static Boolean sIsTablet;

    private static boolean isDesktop() {
        if (sIsDesktop == null) {
            // See the following link for implementation details:
            // https://source.chromium.org/chromium/chromium/src/+/main:base/android/java/src/org/chromium/base/DeviceInfo.java;l=285-287;drc=61d3d9feb38d8045309dd9e237f5305de987523d
            sIsDesktop = DeviceInfo.isDesktop();
        }
        return sIsDesktop;
    }

    /**
     * Whether the form factor is desktop where an app is opened in a freeform window by default.
     *
     * <p>Unlike {@link DeviceInfo#isDesktop()}, the return value can't be overridden by {@code
     * BaseSwitches.FORCE_DESKTOP_ANDROID}.
     *
     * <p>This method is only for restricting multi-window tests on {@link
     * DeviceFormFactor#DESKTOP_FREEFORM}. To avoid confusion in production code, we don't add it as
     * an API of {@link DeviceInfo}.
     */
    static boolean isDesktopFreeform() {
        if (sIsDesktopFreeform == null) {
            var packageManager = ContextUtils.getApplicationContext().getPackageManager();
            sIsDesktopFreeform =
                    BuildConfig.IS_DESKTOP_ANDROID
                            && packageManager.hasSystemFeature(PackageManager.FEATURE_PC);
        }

        return sIsDesktopFreeform;
    }

    private static boolean isTablet() {
        if (sIsTablet == null) {
            sIsTablet =
                    ThreadUtils.runOnUiThreadBlocking(
                            () ->
                                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                                            InstrumentationRegistry.getInstrumentation()
                                                    .getTargetContext()));
        }
        return sIsTablet;
    }

    public static void registerChecks(RestrictionSkipCheck check) {
        check.addHandler(DeviceFormFactor.DESKTOP, () -> !isDesktop());
        check.addHandler(DeviceFormFactor.DESKTOP_FREEFORM, () -> !isDesktopFreeform());

        // isTablet() returns True if the display is large enough to be considered a tablet, so
        // it is always True on desktop devices as well.
        check.addHandler(DeviceFormFactor.PHONE, () -> isDesktop() || isTablet());
        check.addHandler(DeviceFormFactor.ONLY_TABLET, () -> isDesktop() || !isTablet());
        check.addHandler(DeviceFormFactor.TABLET_OR_DESKTOP, () -> !isTablet());
        check.addHandler(DeviceFormFactor.PHONE_OR_TABLET, () -> isDesktop());
    }
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * UiRestrictions list form factor restrictions, that are usable with the {@link Restriction}
 * annotation in layers depending on //ui. E.g. <code>
 *   \@Restriction({DeviceFormFactor.RESTRICTION_TYPE_PHONE})
 * </code>
 */
public final class UiRestriction {
    /** Specifies the test is only valid on phone sized screens. */
    public static final String RESTRICTION_TYPE_PHONE = DeviceFormFactor.PHONE;

    /** Specifies the test is only valid on tablet sized screens. */
    public static final String RESTRICTION_TYPE_TABLET = DeviceFormFactor.TABLET;

    private static Boolean sIsTablet;

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
        check.addHandler(DeviceFormFactor.PHONE, () -> isTablet());
        check.addHandler(DeviceFormFactor.TABLET, () -> !isTablet());
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RestrictionSkipCheck;

/**
 * DeviceRestrictions list device form factor restrictions, that are usable with the {@link
 * Restriction} annotation in layers depending on device type. E.g. <code>
 *   \@Restriction({DeviceRestriction.RESTRICTION_TYPE_AUTO})
 * </code>
 */
public final class DeviceRestriction {
    /** Specifies the test is only valid on automotive form factors. */
    public static final String RESTRICTION_TYPE_AUTO = "Auto";

    /** Specifies the test is only valid on non-automotive form factors. */
    public static final String RESTRICTION_TYPE_NON_AUTO = "Non Auto";

    private static Boolean sIsAuto;

    private static boolean isAuto() {
        if (sIsAuto == null) {
            sIsAuto = ThreadUtils.runOnUiThreadBlocking(() -> BuildInfo.getInstance().isAutomotive);
        }
        return sIsAuto;
    }

    public static void registerChecks(RestrictionSkipCheck check) {
        check.addHandler(RESTRICTION_TYPE_AUTO, () -> !isAuto());
        check.addHandler(RESTRICTION_TYPE_NON_AUTO, () -> isAuto());
    }
}

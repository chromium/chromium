// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RestrictionSkipCheck;

/**
 * GmsCoreVersionRestriction list form factor restrictions, that are usable with the {@link
 * Restriction} annotation in layers depending on //ui. E.g. <code>
 *   \@Restriction({GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W24})
 * </code>
 */
public final class GmsCoreVersionRestriction {
    /** Specifies the test to run only with the GMS Core version greater or equal 22w30. */
    public static final String RESTRICTION_TYPE_VERSION_GE_22W30 = "GMSCoreVersion22w30";

    /** Specifies the test to run only with the GMS Core version greater or equal 2020w02. */
    public static final String RESTRICTION_TYPE_VERSION_GE_2020W02 = "GMSCoreVersion2020w02";

    private static final int VERSION_2020W02 = 20415000;
    private static final int VERSION_22W30 = 223012000;
    private static Integer sGmsVersion;

    public static void registerChecks(RestrictionSkipCheck check) {
        check.addHandler(RESTRICTION_TYPE_VERSION_GE_22W30, () -> getVersion() < VERSION_22W30);
        check.addHandler(RESTRICTION_TYPE_VERSION_GE_2020W02, () -> getVersion() < VERSION_2020W02);
    }

    private static int tryParseInt(String value, int defaultVal) {
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return defaultVal;
        }
    }

    private static int getVersion() {
        if (sGmsVersion == null) {
            String gmsVersionStr =
                    ThreadUtils.runOnUiThreadBlockingNoException(
                            () -> BuildInfo.getInstance().gmsVersionCode);
            sGmsVersion = tryParseInt(gmsVersionStr, 0);
        }
        return sGmsVersion;
    }
}

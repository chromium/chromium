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
    /** Specifies the test to run only with the GMS Core version greater than or equal to 24w15. */
    public static final String RESTRICTION_TYPE_VERSION_GE_24W15 = "GMSCoreVersion24w15";

    /** Specifies the test to run only with the GMS Core version greater than or equal to 23w12. */
    public static final String RESTRICTION_TYPE_VERSION_GE_23W12 = "GMSCoreVersion23w12";

    /** Specifies the test to run only with the GMS Core version greater than or equal to 22w30. */
    public static final String RESTRICTION_TYPE_VERSION_GE_22W30 = "GMSCoreVersion22w30";

    /** Specifies the test to run only with the GMS Core version greater than or equal to 20w02. */
    public static final String RESTRICTION_TYPE_VERSION_GE_20W02 = "GMSCoreVersion20w02";

    /** Specifies the test to run only with the GMS Core version greater than or equal to 19w13. */
    public static final String RESTRICTION_TYPE_VERSION_GE_19W13 = "GMSCoreVersion19w13";

    private static final int VERSION_24W15 = 241512000;
    private static final int VERSION_23W12 = 231206000;
    private static final int VERSION_22W30 = 223012000;
    private static final int VERSION_20W02 = 20415000;
    private static final int VERSION_19W13 = 16890000;

    private static Integer sGmsVersion;

    public static void registerChecks(RestrictionSkipCheck check) {
        check.addHandler(RESTRICTION_TYPE_VERSION_GE_24W15, () -> getVersion() < VERSION_24W15);
        check.addHandler(RESTRICTION_TYPE_VERSION_GE_23W12, () -> getVersion() < VERSION_23W12);
        check.addHandler(RESTRICTION_TYPE_VERSION_GE_22W30, () -> getVersion() < VERSION_22W30);
        check.addHandler(RESTRICTION_TYPE_VERSION_GE_20W02, () -> getVersion() < VERSION_20W02);
        check.addHandler(RESTRICTION_TYPE_VERSION_GE_19W13, () -> getVersion() < VERSION_19W13);
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
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> BuildInfo.getInstance().getGmsVersionCode());
            sGmsVersion = tryParseInt(gmsVersionStr, 0);
        }
        return sGmsVersion;
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RestrictionSkipCheck;

/** Checks if any restrictions exist and skip the test if it meets those restrictions. */
public class GmsCoreVersionRestrictionSkipCheck extends RestrictionSkipCheck {
    private static final int VERSION_22W30 = 223012000;

    public GmsCoreVersionRestrictionSkipCheck(Context targetContext) {
        super(targetContext);
    }

    @Override
    protected boolean restrictionApplies(String restriction) {
        boolean restrictedToGmsVersion =
                TextUtils.equals(
                        restriction, GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30);
        if (!restrictedToGmsVersion) {
            return false;
        }

        String gmsVersionStr =
                ThreadUtils.runOnUiThreadBlockingNoException(
                        () -> BuildInfo.getInstance().gmsVersionCode);
        int version = tryParseInt(gmsVersionStr, 0);
        return version < VERSION_22W30;
    }

    private static int tryParseInt(String value, int defaultVal) {
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return defaultVal;
        }
    }
}

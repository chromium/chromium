// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Checks if any restrictions exist and skip the test if it meets those restrictions.
 */
public class UiRestrictionSkipCheck extends RestrictionSkipCheck {
    public UiRestrictionSkipCheck(Context targetContext) {
        super(targetContext);
    }

    @Override
    protected boolean restrictionApplies(String restriction) {
        boolean phoneOnly = TextUtils.equals(restriction, UiRestriction.RESTRICTION_TYPE_PHONE);
        boolean tabletOnly = TextUtils.equals(restriction, UiRestriction.RESTRICTION_TYPE_TABLET);
        if (!phoneOnly && !tabletOnly) {
            return false;
        }
        boolean isTablet =
                ThreadUtils.runOnUiThreadBlockingNoException(() -> DeviceFormFactor.isTablet());
        return phoneOnly && isTablet || tabletOnly && !isTablet;
    }
}

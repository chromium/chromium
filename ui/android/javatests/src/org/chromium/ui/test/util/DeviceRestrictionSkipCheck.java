// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.RestrictionSkipCheck;

/**
 * Checks if any restrictions exist and skip the test if it meets those restrictions.
 */
public class DeviceRestrictionSkipCheck extends RestrictionSkipCheck {
    public DeviceRestrictionSkipCheck(Context targetContext) {
        super(targetContext);
    }

    @Override
    protected boolean restrictionApplies(String restriction) {
        boolean restrictedToAuto =
                TextUtils.equals(restriction, DeviceRestriction.RESTRICTION_TYPE_AUTO);
        boolean restrictedToNonAuto =
                TextUtils.equals(restriction, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO);

        boolean noRestrictionsApply = !restrictedToAuto && !restrictedToNonAuto;
        if (noRestrictionsApply) {
            return false;
        }

        boolean isAuto = ThreadUtils.runOnUiThreadBlockingNoException(
                () -> BuildInfo.getInstance().isAutomotive);
        boolean autoRestrictionApplies = restrictedToAuto && !isAuto;
        boolean nonAutoRestrictionApplies = restrictedToNonAuto && isAuto;
        return autoRestrictionApplies || nonAutoRestrictionApplies;
    }
}

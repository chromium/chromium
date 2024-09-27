// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisableIfSkipCheck;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Checks for conditional disables. Currently only includes checks against a few device form factor
 * values.
 */
public class UiDisableIfSkipCheck extends DisableIfSkipCheck {
    private final Context mTargetContext;

    public UiDisableIfSkipCheck(Context targetContext) {
        mTargetContext = targetContext;
    }

    @Override
    protected boolean deviceTypeApplies(String type) {
        final boolean phoneOnly = TextUtils.equals(type, DeviceFormFactor.PHONE);
        final boolean tabletOnly = TextUtils.equals(type, DeviceFormFactor.TABLET);
        if (!phoneOnly && !tabletOnly) {
            return false;
        }
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean isTablet =
                            DeviceFormFactor.isNonMultiDisplayContextOnTablet(mTargetContext);
                    return (phoneOnly && !isTablet) || (tabletOnly && isTablet);
                });
    }
}

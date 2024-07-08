// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.transit;

import android.app.Activity;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.ui.KeyboardUtils;

/** Expects the soft keyboard to be in the expected state (shown or hidden). */
public class SoftKeyboardCondition extends ConditionWithResult<Void> {

    private final Supplier<? extends Activity> mActivitySupplier;
    private final boolean mExpectShowing;

    public SoftKeyboardCondition(
            Supplier<? extends Activity> activitySupplier, boolean expectShowing) {
        super(/* isRunOnUiThread= */ false);
        mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        mExpectShowing = expectShowing;
    }

    @Override
    protected ConditionStatusWithResult<Void> resolveWithSuppliers() {
        View view = mActivitySupplier.get().getWindow().getDecorView();
        return whether(KeyboardUtils.isAndroidSoftKeyboardShowing(view) == mExpectShowing)
                .withoutResult();
    }

    @Override
    public String buildDescription() {
        if (mExpectShowing) {
            return "Soft keyboard is showing";
        } else {
            return "Soft keyboard is hidden";
        }
    }
}

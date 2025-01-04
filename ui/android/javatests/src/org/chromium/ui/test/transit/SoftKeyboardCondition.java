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

/**
 * Expects the soft keyboard to be in the expected state (shown or hidden).
 *
 * <p>The result is whether the soft keyboard was expected to show (if there is a physical keyboard
 * and show_ime_with_hard_keyboard is off, the keyboard soft should not be shown).
 */
public class SoftKeyboardCondition extends ConditionWithResult<Boolean> {
    private final Supplier<? extends Activity> mActivitySupplier;
    private final boolean mExpectShowingWhenNoPhysicalKeyboard;
    private Boolean mShouldSoftKeyboardShow;

    public SoftKeyboardCondition(
            Supplier<? extends Activity> activitySupplier, boolean expectShowing) {
        super(/* isRunOnUiThread= */ false);
        mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        mExpectShowingWhenNoPhysicalKeyboard = expectShowing;
    }

    @Override
    protected ConditionStatusWithResult<Boolean> resolveWithSuppliers() {
        Activity activity = mActivitySupplier.get();
        if (mShouldSoftKeyboardShow == null) {
            mShouldSoftKeyboardShow = KeyboardUtils.isSoftKeyboardEnabled(activity);
            rebuildDescription();
        }

        if (!mShouldSoftKeyboardShow) {
            return fulfilled(
                            "Soft keyboard not expected because hard keyboard is connected and"
                                    + " show_ime_with_hard_keyboard is off")
                    .withResult(false);
        }

        View view = activity.getWindow().getDecorView();
        return whether(
                        KeyboardUtils.isAndroidSoftKeyboardShowing(view)
                                == mExpectShowingWhenNoPhysicalKeyboard)
                .withResult(true);
    }

    @Override
    public String buildDescription() {
        if (mExpectShowingWhenNoPhysicalKeyboard) {
            if (mShouldSoftKeyboardShow == null) {
                return "Soft keyboard is in the right state";
            } else {
                if (mShouldSoftKeyboardShow) {
                    return "Soft keyboard is showing";
                } else {
                    return "Soft keyboard is not expected to show";
                }
            }
        } else {
            return "Soft keyboard is hidden";
        }
    }
}

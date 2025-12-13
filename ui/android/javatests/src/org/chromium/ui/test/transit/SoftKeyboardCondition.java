// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.transit;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.provider.Settings;
import android.view.View;

import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/**
 * Expects the soft keyboard to be in the expected state (shown or hidden).
 *
 * <p>The result is whether the soft keyboard was expected to show (if there is a physical keyboard
 * and show_ime_with_hard_keyboard is off, the keyboard soft should not be shown).
 */
public class SoftKeyboardCondition extends ConditionWithResult<Boolean> {
    private final Supplier<? extends Activity> mActivitySupplier;
    private final boolean mExpectShowingWhenNoPhysicalKeyboard;
    private @Nullable Boolean mShouldSoftKeyboardShow;
    private @Nullable String mShouldSoftKeyboardShowReason;

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
            determineIfSoftKeyboardShouldShow(activity);
            rebuildDescription();
        }

        if (!mShouldSoftKeyboardShow) {
            return fulfilled().withResult(false);
        }

        View view = activity.getWindow().getDecorView();
        return whether(
                        KeyboardUtils.isAndroidSoftKeyboardShowing(view)
                                == mExpectShowingWhenNoPhysicalKeyboard)
                .withResult(true);
    }

    /**
     * Determines whether the soft keyboard is expected to show when keyboard input is required.
     *
     * <p>When there is a physical keyboard and show_ime_with_hard_keyboard is off, the soft
     * keyboard is not shown.
     */
    private void determineIfSoftKeyboardShouldShow(Context context) {
        if (!KeyboardUtils.isHardKeyboardConnected(context)) {
            mShouldSoftKeyboardShow = true;
            mShouldSoftKeyboardShowReason = "no hard keyboard";
            return;
        } else {
            mShouldSoftKeyboardShowReason = "hard keyboard connected";
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            // In Android 16+, |show_ime_with_hard_keyboard| does not exist anymore. Whether the
            // soft keyboard shows up depends on gboard settings which we cannot access, therefore
            // we need to assume.
            //
            // In both devices and emulators, we force the soft keyboard to show up by
            // changing Gboard settings.
            mShouldSoftKeyboardShow = true;
            mShouldSoftKeyboardShowReason += " and soft keyboard was forced to show";
        } else {
            // In Android 15-, |show_ime_with_hard_keyboard| determines whether the soft
            // keyboard shows up when there is a hardware keyboard connected.
            int showImeWithHardKeyboard =
                    Settings.Secure.getInt(
                            context.getContentResolver(), "show_ime_with_hard_keyboard", 0);
            mShouldSoftKeyboardShow = showImeWithHardKeyboard != 0;
            mShouldSoftKeyboardShowReason +=
                    " and show_ime_with_hard_keyboard is " + showImeWithHardKeyboard;
        }
    }

    @Override
    public String buildDescription() {
        if (mExpectShowingWhenNoPhysicalKeyboard) {
            if (mShouldSoftKeyboardShow == null) {
                return "Soft keyboard is in the right state";
            } else {
                if (mShouldSoftKeyboardShow) {
                    return "Soft keyboard is showing since " + mShouldSoftKeyboardShowReason;
                } else {
                    return "Soft keyboard is not expected to show since "
                            + mShouldSoftKeyboardShowReason;
                }
            }
        } else {
            return "Soft keyboard is hidden";
        }
    }
}

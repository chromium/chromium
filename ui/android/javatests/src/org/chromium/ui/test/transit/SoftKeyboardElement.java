// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.transit;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Element;

/** Represents the soft keyboard shown, expecting it to hide after exiting the ConditionalState. */
public class SoftKeyboardElement extends Element<Void> {

    private final Supplier<? extends Activity> mActivitySupplier;

    public SoftKeyboardElement(Supplier<? extends Activity> activitySupplier) {
        super("SoftKeyboardElement");
        mActivitySupplier = activitySupplier;
    }

    @Override
    public ConditionWithResult<Void> createEnterCondition() {
        return new SoftKeyboardCondition(mActivitySupplier, /* expectShowing= */ true);
    }

    @Override
    public Condition createExitCondition() {
        return new SoftKeyboardCondition(mActivitySupplier, /* expectShowing= */ false);
    }
}

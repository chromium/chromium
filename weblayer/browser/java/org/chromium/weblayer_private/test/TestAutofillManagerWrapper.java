// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.autofill.AutofillValue;

import org.chromium.components.autofill.AutofillManagerWrapper;

import java.util.ArrayList;

/**
 * A test AutofillManagerWrapper for AutofillTest
 */
public class TestAutofillManagerWrapper extends AutofillManagerWrapper {
    // There is another copy of the list of the constants below in {@link
    // org.chromium.weblayer.test.AutofillTest}, need to change the corresponding ones in
    // in AutofillTest if any of them are changed.
    public static final int AUTOFILL_VIEW_ENTERED = 1;
    public static final int AUTOFILL_VIEW_EXITED = 2;
    public static final int AUTOFILL_VALUE_CHANGED = 3;
    public static final int AUTOFILL_COMMIT = 4;
    public static final int AUTOFILL_CANCEL = 5;
    public static final int AUTOFILL_SESSION_STARTED = 6;
    public static final int AUTOFILL_QUERY_DONE = 7;

    public TestAutofillManagerWrapper(
            Context context, Runnable onNewEvents, ArrayList<Integer> eventsObserved) {
        super(context);
        mOnNewEvents = onNewEvents;
        mEventsObserved = eventsObserved;
    }

    @Override
    public boolean isDisabled() {
        return false;
    }

    @Override
    public boolean isAwGCurrentAutofillService() {
        return true;
    }

    @Override
    public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        mEventsObserved.add(AUTOFILL_VIEW_ENTERED);
        mOnNewEvents.run();
    }

    @Override
    public void notifyVirtualViewExited(View parent, int childId) {
        mEventsObserved.add(AUTOFILL_VIEW_EXITED);
        mOnNewEvents.run();
    }

    @Override
    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        mEventsObserved.add(AUTOFILL_VALUE_CHANGED);
        mOnNewEvents.run();
    }

    @Override
    public void commit(int submissionSource) {
        mEventsObserved.add(AUTOFILL_COMMIT);
        mOnNewEvents.run();
    }

    @Override
    public void cancel() {
        mEventsObserved.add(AUTOFILL_CANCEL);
        mOnNewEvents.run();
    }

    @Override
    public void notifyNewSessionStarted(boolean hasServerPrediction) {
        mEventsObserved.add(AUTOFILL_SESSION_STARTED);
        mOnNewEvents.run();
    }

    @Override
    public void onQueryDone(boolean success) {
        mEventsObserved.add(AUTOFILL_QUERY_DONE);
        mOnNewEvents.run();
    }

    private ArrayList<Integer> mEventsObserved;
    private Runnable mOnNewEvents;
}

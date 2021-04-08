// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.autofill.AutofillValue;

import org.chromium.components.autofill.AutofillManagerWrapper;
import org.chromium.weblayer_private.test_interfaces.AutofillEventType;

import java.util.ArrayList;

/**
 * A test AutofillManagerWrapper for AutofillTest
 */
public class TestAutofillManagerWrapper extends AutofillManagerWrapper {
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
        mEventsObserved.add(AutofillEventType.VIEW_ENTERED);
        mOnNewEvents.run();
    }

    @Override
    public void notifyVirtualViewExited(View parent, int childId) {
        mEventsObserved.add(AutofillEventType.VIEW_EXITED);
        mOnNewEvents.run();
    }

    @Override
    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        mEventsObserved.add(AutofillEventType.VALUE_CHANGED);
        mOnNewEvents.run();
    }

    @Override
    public void commit(int submissionSource) {
        mEventsObserved.add(AutofillEventType.COMMIT);
        mOnNewEvents.run();
    }

    @Override
    public void cancel() {
        mEventsObserved.add(AutofillEventType.CANCEL);
        mOnNewEvents.run();
    }

    @Override
    public void notifyNewSessionStarted(boolean hasServerPrediction) {
        mEventsObserved.add(AutofillEventType.SESSION_STARTED);
        mOnNewEvents.run();
    }

    @Override
    public void onQueryDone(boolean success) {
        mEventsObserved.add(AutofillEventType.QUERY_DONE);
        mOnNewEvents.run();
    }

    private ArrayList<Integer> mEventsObserved;
    private Runnable mOnNewEvents;
}

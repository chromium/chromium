// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.IntentSender;
import android.content.IntentSender.SendIntentException;

import org.chromium.ui.base.IntentRequestTracker.Delegate;

import java.lang.ref.WeakReference;

/** Chrome's implementation of the delegate of a IntentRequestTracker. */
public class ActivityIntentRequestTrackerDelegate implements Delegate {
    // Just create one ImmutableWeakReference object to avoid gc churn.
    private final ImmutableWeakReference<Activity> mActivityWeakRefHolder;

    /**
     * Create an instance of delegate for the given activity that will own the IntentRequestTracker.
     * @param activity The activity to own the IntentRequestTracker.
     */
    public ActivityIntentRequestTrackerDelegate(Activity activity) {
        assert activity != null;
        mActivityWeakRefHolder = new ImmutableWeakReference<>(activity);
    }

    @Override
    public boolean startActivityForResult(Intent intent, int requestCode) {
        Activity activity = mActivityWeakRefHolder.get();
        if (activity == null) return false;
        try {
            activity.startActivityForResult(intent, requestCode);
        } catch (ActivityNotFoundException e) {
            return false;
        }
        return true;
    }

    @Override
    public boolean startIntentSenderForResult(IntentSender intentSender, int requestCode) {
        Activity activity = mActivityWeakRefHolder.get();
        if (activity == null) return false;
        try {
            activity.startIntentSenderForResult(intentSender, requestCode, new Intent(), 0, 0, 0);
        } catch (SendIntentException e) {
            return false;
        }
        return true;
    }

    @Override
    public void finishActivity(int requestCode) {
        Activity activity = mActivityWeakRefHolder.get();
        if (activity == null) return;
        activity.finishActivity(requestCode);
    }

    @Override
    public final WeakReference<Activity> getActivity() {
        return mActivityWeakRefHolder;
    }
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.chromium.ui.base.WindowAndroid.START_INTENT_FAILURE;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;
import android.util.SparseArray;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.lang.ref.WeakReference;
import java.util.HashMap;

/** The implementation of IntentRequestTracker. */
/* package */ final class IntentRequestTrackerImpl implements IntentRequestTracker {
    // Constants used for intent request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;

    private final SparseArray<IntentCallback> mOutstandingIntents;
    private int mNextRequestCode;
    private final Delegate mDelegate;

    // Ideally, this would be a SparseArray<String>, but there's no easy way to store a
    // SparseArray<String> in a bundle during saveInstanceState(). So we use a HashMap and suppress
    // the Android lint warning "UseSparseArrays".
    private HashMap<Integer, String> mIntentErrors;

    /**
     * Creates an instance of the class.
     * @param delegate The delegate that wraps the activity that owns the tracker.
     */
    /* package */ IntentRequestTrackerImpl(Delegate delegate) {
        mOutstandingIntents = new SparseArray<>();
        mIntentErrors = new HashMap<>();
        mDelegate = delegate;
    }

    /* package */ int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        int requestCode = generateNextRequestCode();

        if (!mDelegate.startIntentSenderForResult(intent.getIntentSender(), requestCode)) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    /* package */ int showCancelableIntent(
            Intent intent, IntentCallback callback, Integer errorId) {
        int requestCode = generateNextRequestCode();

        if (!mDelegate.startActivityForResult(intent, requestCode)) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    /* package */ int showCancelableIntent(
            Callback<Integer> intentTrigger, IntentCallback callback, Integer errorId) {
        int requestCode = generateNextRequestCode();

        intentTrigger.onResult(requestCode);

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    /* package */ void cancelIntent(int requestCode) {
        mDelegate.finishActivity(requestCode);
    }

    /* package */ boolean removeIntentCallback(IntentCallback callback) {
        int requestCode = getOutstandingIntents().indexOfValue(callback);
        if (requestCode < 0) return false;
        getOutstandingIntents().remove(requestCode);
        mIntentErrors.remove(requestCode);
        return true;
    }

    @Override
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        IntentCallback callback = getOutstandingIntents().get(requestCode);
        getOutstandingIntents().delete(requestCode);
        String errorMessage = mIntentErrors.remove(requestCode);

        if (callback != null) {
            callback.onIntentCompleted(resultCode, data);
            return true;
        } else {
            if (errorMessage != null && !mDelegate.onCallbackNotFoundError(errorMessage)) {
                WindowAndroid.showError(errorMessage);
            }
        }
        return false;
    }

    @Override
    public WeakReference<Activity> getActivity() {
        return mDelegate.getActivity();
    }

    @Override
    public void saveInstanceState(Bundle bundle) {
        bundle.putSerializable(WindowAndroid.WINDOW_CALLBACK_ERRORS, mIntentErrors);
    }

    @Override
    public void restoreInstanceState(Bundle bundle) {
        if (bundle == null) return;

        Object errors = bundle.getSerializable(WindowAndroid.WINDOW_CALLBACK_ERRORS);
        if (errors instanceof HashMap) {
            @SuppressWarnings("unchecked")
            HashMap<Integer, String> intentErrors = (HashMap<Integer, String>) errors;
            mIntentErrors = intentErrors;
        }
    }

    private int generateNextRequestCode() {
        int requestCode = REQUEST_CODE_PREFIX + mNextRequestCode;
        mNextRequestCode = (mNextRequestCode + 1) % REQUEST_CODE_RANGE_SIZE;
        return requestCode;
    }

    private void storeCallbackData(int requestCode, IntentCallback callback, Integer errorId) {
        mOutstandingIntents.put(requestCode, callback);
        mIntentErrors.put(
                requestCode,
                errorId == null ? null : ContextUtils.getApplicationContext().getString(errorId));
    }

    private SparseArray<IntentCallback> getOutstandingIntents() {
        return mOutstandingIntents;
    }
}

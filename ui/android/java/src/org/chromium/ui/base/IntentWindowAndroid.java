// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.util.SparseArray;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;

/**
 * Base class for WindowAndroid implementations that need to send intents.
 */
public abstract class IntentWindowAndroid extends WindowAndroid {
    // Constants used for intent request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;

    private int mNextRequestCode;
    private SparseArray<IntentCallback> mOutstandingIntents;

    public IntentWindowAndroid(Context context) {
        super(context);
        mOutstandingIntents = new SparseArray<>();
    }

    /**
     * Uses the provided intent sender to start the intent.
     * @see Activity#startIntentSenderForResult
     */
    protected abstract boolean startIntentSenderForResult(
            IntentSender intentSender, int requestCode);

    @Override
    public int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        int requestCode = generateNextRequestCode();

        if (!startIntentSenderForResult(intent.getIntentSender(), requestCode)) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    /**
     * Starts an activity for the provided intent.
     * @see Activity#startActivityForResult
     */
    protected abstract boolean startActivityForResult(Intent intent, int requestCode);

    @Override
    public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
        int requestCode = generateNextRequestCode();

        if (!startActivityForResult(intent, requestCode)) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public int showCancelableIntent(
            Callback<Integer> intentTrigger, IntentCallback callback, Integer errorId) {
        Activity activity = getActivity().get();
        if (activity == null) return START_INTENT_FAILURE;

        int requestCode = generateNextRequestCode();

        intentTrigger.onResult(requestCode);

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public void cancelIntent(int requestCode) {
        Activity activity = getActivity().get();
        if (activity == null) return;
        activity.finishActivity(requestCode);
    }

    @Override
    public boolean removeIntentCallback(IntentCallback callback) {
        int requestCode = mOutstandingIntents.indexOfValue(callback);
        if (requestCode < 0) return false;
        mOutstandingIntents.remove(requestCode);
        mIntentErrors.remove(requestCode);
        return true;
    }

    /**
     * Responds to the intent result if the intent was created by the native window.
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     * @return Boolean value of whether the intent was started by the native window.
     */
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        IntentCallback callback = mOutstandingIntents.get(requestCode);
        mOutstandingIntents.delete(requestCode);
        String errorMessage = mIntentErrors.remove(requestCode);

        if (callback != null) {
            callback.onIntentCompleted(this, resultCode, data);
            return true;
        } else {
            if (errorMessage != null) {
                showCallbackNonExistentError(errorMessage);
                return true;
            }
        }
        return false;
    }

    private int generateNextRequestCode() {
        int requestCode = REQUEST_CODE_PREFIX + mNextRequestCode;
        mNextRequestCode = (mNextRequestCode + 1) % REQUEST_CODE_RANGE_SIZE;
        return requestCode;
    }

    private void storeCallbackData(int requestCode, IntentCallback callback, Integer errorId) {
        mOutstandingIntents.put(requestCode, callback);
        mIntentErrors.put(requestCode,
                errorId == null ? null : ContextUtils.getApplicationContext().getString(errorId));
    }
}

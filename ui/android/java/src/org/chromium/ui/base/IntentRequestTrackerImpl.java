// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.chromium.ui.base.WindowAndroid.START_INTENT_FAILURE;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;
import android.util.SparseArray;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.lang.ref.WeakReference;
import java.util.HashMap;

/** The implementation of IntentRequestTracker. */
@NullMarked
public final class IntentRequestTrackerImpl implements IntentRequestTracker {
    // Constants used for intent request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;

    /** A delegate of this class's intent sending. */
    public interface Delegate {
        /**
         * Starts an activity for the provided intent.
         *
         * @see Activity#startActivityForResult
         */
        boolean startActivityForResult(@Nullable Intent intent, int requestCode);

        /**
         * Uses the provided intent sender to start the intent.
         *
         * @see Activity#startIntentSenderForResult
         */
        boolean startIntentSenderForResult(IntentSender intentSender, int requestCode);

        /**
         * Force finish an activity that you had previously started with startActivityForResult or
         * startIntentSenderForResult.
         *
         * @param requestCode The request code returned from showCancelableIntent.
         */
        void finishActivity(int requestCode);

        /**
         * @return A reference to owning Activity. The returned WeakReference will never be null,
         *     but the contained Activity can be null (if it has been garbage collected). The
         *     returned WeakReference is immutable and calling clear will throw an exception.
         */
        WeakReference<Activity> getActivity();

        /**
         * Invoked from {@link #onActivityResult} when an intent's callback is not found.
         *
         * @param error The error message that was set when "showing an intent" was requested.
         * @return True if the error has been handled. Otherwise, the caller needs to handle the
         *     unhandled error.
         */
        boolean onCallbackNotFoundError(String error);
    }

    /**
     * Creates an instance of the class from a given delegate.
     *
     * @param delegate A delegate that will be responsible for intent sending.
     * @return an instance of the class.
     */
    public static IntentRequestTracker createFromDelegate(Delegate delegate) {
        return new IntentRequestTrackerImpl(delegate);
    }

    /**
     * Creates an instance of the class from a given activity. This is a pure convenience method to
     * create with an ActivityIntentRequestTrackerDelegate. Clients that needs to customize the
     * Delegate should use {@link #createFromDelegate} instead.
     *
     * @param activity The activity who will be used as the intent sender and whose intent requests
     *     to be tracked.
     * @return an instance of the class.
     */
    public static IntentRequestTrackerImpl createFromActivity(Activity activity) {
        return new IntentRequestTrackerImpl(new ActivityIntentRequestTrackerDelegate(activity));
    }

    private final SparseArray<@Nullable IntentCallback> mOutstandingIntents;
    private int mNextRequestCode;
    private final Delegate mDelegate;

    // Ideally, this would be a SparseArray<String>, but there's no easy way to store a
    // SparseArray<String> in a bundle during saveInstanceState(). So we use a HashMap and suppress
    // the Android lint warning "UseSparseArrays".
    private HashMap<Integer, @Nullable String> mIntentErrors;

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
            PendingIntent intent, @Nullable IntentCallback callback, @Nullable Integer errorId) {
        int requestCode = generateNextRequestCode();

        if (!mDelegate.startIntentSenderForResult(intent.getIntentSender(), requestCode)) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public int showCancelableIntent(
            @Nullable Intent intent, @Nullable IntentCallback callback, @Nullable Integer errorId) {
        int requestCode = generateNextRequestCode();

        if (!mDelegate.startActivityForResult(intent, requestCode)) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    /* package */ int showCancelableIntent(
            Callback<Integer> intentTrigger,
            @Nullable IntentCallback callback,
            @Nullable Integer errorId) {
        int requestCode = generateNextRequestCode();

        intentTrigger.onResult(requestCode);

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    /* package */ void cancelIntent(int requestCode) {
        mDelegate.finishActivity(requestCode);
    }

    /* package */ boolean removeIntentCallback(IntentCallback callback) {
        int requestCode = mOutstandingIntents.indexOfValue(callback);
        if (requestCode < 0) return false;
        mOutstandingIntents.remove(requestCode);
        mIntentErrors.remove(requestCode);
        return true;
    }

    @Override
    public boolean onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        IntentCallback callback = mOutstandingIntents.get(requestCode);
        mOutstandingIntents.delete(requestCode);
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
    public void restoreInstanceState(@Nullable Bundle bundle) {
        if (bundle == null) return;

        Object errors = bundle.getSerializable(WindowAndroid.WINDOW_CALLBACK_ERRORS);
        if (errors instanceof HashMap) {
            @SuppressWarnings("unchecked")
            var intentErrors = (HashMap<Integer, @Nullable String>) errors;
            mIntentErrors = intentErrors;
        }
    }

    private int generateNextRequestCode() {
        int requestCode = REQUEST_CODE_PREFIX + mNextRequestCode;
        mNextRequestCode = (mNextRequestCode + 1) % REQUEST_CODE_RANGE_SIZE;
        return requestCode;
    }

    private void storeCallbackData(
            int requestCode, @Nullable IntentCallback callback, @Nullable Integer errorId) {
        mOutstandingIntents.put(requestCode, callback);
        mIntentErrors.put(
                requestCode,
                errorId == null ? null : ContextUtils.getApplicationContext().getString(errorId));
    }
}

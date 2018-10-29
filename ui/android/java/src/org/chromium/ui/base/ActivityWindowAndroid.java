// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;

import java.lang.ref.WeakReference;

/**
 * The class provides the WindowAndroid's implementation which requires
 * Activity Instance.
 * Only instantiate this class when you need the implemented features.
 */
public class ActivityWindowAndroid
        extends WindowAndroid implements ApplicationStatus.ActivityStateListener {
    // Constants used for intent request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;

    private int mNextRequestCode;

    private boolean mListenToActivityState;

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * TODO(jdduke): Remove this overload when all callsites have been updated to
     * indicate their activity state listening preference.
     * @param context Context wrapping an activity associated with the WindowAndroid.
     */
    public ActivityWindowAndroid(Context context) {
        this(context, true);
    }

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * @param context Context wrapping an activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     */
    public ActivityWindowAndroid(Context context, boolean listenToActivityState) {
        super(context);
        Activity activity = activityFromContext(context);
        if (activity == null) {
            throw new IllegalArgumentException("Context is not and does not wrap an Activity");
        }
        mListenToActivityState = listenToActivityState;
        if (listenToActivityState) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
        }

        setKeyboardDelegate(createKeyboardVisibilityDelegate());
        setAndroidPermissionDelegate(createAndroidPermissionDelegate());
    }

    protected ActivityAndroidPermissionDelegate createAndroidPermissionDelegate() {
        return new ActivityAndroidPermissionDelegate(getActivity());
    }

    protected ActivityKeyboardVisibilityDelegate createKeyboardVisibilityDelegate() {
        return new ActivityKeyboardVisibilityDelegate(getActivity());
    }

    @Override
    public ActivityKeyboardVisibilityDelegate getKeyboardDelegate() {
        return (ActivityKeyboardVisibilityDelegate) super.getKeyboardDelegate();
    }

    @Override
    public int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        Activity activity = getActivity().get();
        if (activity == null) return START_INTENT_FAILURE;

        int requestCode = generateNextRequestCode();

        try {
            activity.startIntentSenderForResult(
                    intent.getIntentSender(), requestCode, new Intent(), 0, 0, 0);
        } catch (SendIntentException e) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
        Activity activity = getActivity().get();
        if (activity == null) return START_INTENT_FAILURE;

        int requestCode = generateNextRequestCode();

        try {
            activity.startActivityForResult(intent, requestCode);
        } catch (ActivityNotFoundException e) {
            return START_INTENT_FAILURE;
        }

        storeCallbackData(requestCode, callback, errorId);
        return requestCode;
    }

    @Override
    public int showCancelableIntent(Callback<Integer> intentTrigger, IntentCallback callback,
            Integer errorId) {
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

    @Override
    public WeakReference<Activity> getActivity() {
        return new WeakReference<>(activityFromContext(getContext().get()));
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STOPPED) {
            onActivityStopped();
        } else if (newState == ActivityState.STARTED) {
            onActivityStarted();
        } else if (newState == ActivityState.PAUSED) {
            onActivityPaused();
        } else if (newState == ActivityState.RESUMED) {
            onActivityResumed();
        }
    }

    @Override
    @ActivityState
    public int getActivityState() {
        return mListenToActivityState ? ApplicationStatus.getStateForActivity(getActivity().get())
                                      : super.getActivityState();
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

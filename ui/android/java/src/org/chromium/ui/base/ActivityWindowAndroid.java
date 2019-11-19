// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.IntentSender.SendIntentException;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;

import java.lang.ref.WeakReference;

/**
 * The class provides the WindowAndroid's implementation which requires
 * Activity Instance.
 * Only instantiate this class when you need the implemented features.
 */
public class ActivityWindowAndroid
        extends IntentWindowAndroid implements ApplicationStatus.ActivityStateListener {
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
        Activity activity = ContextUtils.activityFromContext(context);
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
    protected final boolean startIntentSenderForResult(IntentSender intentSender, int requestCode) {
        Activity activity = getActivity().get();
        if (activity == null) return false;

        try {
            activity.startIntentSenderForResult(intentSender, requestCode, new Intent(), 0, 0, 0);
        } catch (SendIntentException e) {
            return false;
        }
        return true;
    }

    @Override
    protected final boolean startActivityForResult(Intent intent, int requestCode) {
        Activity activity = getActivity().get();
        if (activity == null) return false;

        try {
            activity.startActivityForResult(intent, requestCode);
        } catch (ActivityNotFoundException e) {
            return false;
        }
        return true;
    }

    @Override
    public WeakReference<Activity> getActivity() {
        return new WeakReference<>(ContextUtils.activityFromContext(getContext().get()));
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
}

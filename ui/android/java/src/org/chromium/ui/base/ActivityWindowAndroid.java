// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.CachedActivityAndroidPermissionDelegate;

import java.lang.ref.WeakReference;

/**
 * The class provides the WindowAndroid's implementation which requires
 * Activity Instance.
 * Only instantiate this class when you need the implemented features.
 */
public class ActivityWindowAndroid
        extends WindowAndroid implements ApplicationStatus.ActivityStateListener {
    private final boolean mListenToActivityState;

    // Just create one ImmutableWeakReference object to avoid gc churn.
    private ImmutableWeakReference<Activity> mActivityWeakRefHolder;

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * @param context Context wrapping an activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ActivityWindowAndroid(Context context, boolean listenToActivityState,
            IntentRequestTracker intentRequestTracker) {
        this(context, listenToActivityState,
                new ActivityKeyboardVisibilityDelegate(
                        new WeakReference<Activity>(ContextUtils.activityFromContext(context))),
                intentRequestTracker);
    }

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     * @param context Context wrapping an activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     * @param activityAndroidPermissionDelegate Delegates which handles android permissions.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ActivityWindowAndroid(Context context, boolean listenToActivityState,
            ActivityKeyboardVisibilityDelegate activityKeyboardVisibilityDelegate,
            IntentRequestTracker intentRequestTracker) {
        super(context, intentRequestTracker);

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) {
            throw new IllegalArgumentException("Context is not and does not wrap an Activity");
        }

        mListenToActivityState = listenToActivityState;

        if (listenToActivityState) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
        }

        WeakReference<Activity> activityWeakReference = new WeakReference<Activity>(activity);

        ActivityAndroidPermissionDelegate activityAndroidPermissionDelegate = listenToActivityState
                ? new CachedActivityAndroidPermissionDelegate(activityWeakReference)
                : new ActivityAndroidPermissionDelegate(activityWeakReference);

        setKeyboardDelegate(activityKeyboardVisibilityDelegate);

        setAndroidPermissionDelegate(activityAndroidPermissionDelegate);
    }

    @Override
    public ActivityKeyboardVisibilityDelegate getKeyboardDelegate() {
        return (ActivityKeyboardVisibilityDelegate) super.getKeyboardDelegate();
    }

    @Override
    public WeakReference<Activity> getActivity() {
        if (mActivityWeakRefHolder == null) {
            mActivityWeakRefHolder = new ImmutableWeakReference<>(
                    ContextUtils.activityFromContext(getContext().get()));
        }
        return mActivityWeakRefHolder;
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
        } else if (newState == ActivityState.DESTROYED) {
            onActivityDestroyed();
        }
    }

    @Override
    @ActivityState
    public int getActivityState() {
        return mListenToActivityState ? ApplicationStatus.getStateForActivity(getActivity().get())
                                      : super.getActivityState();
    }
}

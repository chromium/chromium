// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;

import java.lang.ref.WeakReference;

/**
 * The class provides the WindowAndroid's implementation which requires Activity Instance. Only
 * instantiate this class when you need the implemented features.
 */
public class ActivityWindowAndroid extends WindowAndroid
        implements ApplicationStatus.ActivityStateListener,
                ApplicationStatus.WindowFocusChangedListener {
    private final boolean mListenToActivityState;

    // Just create one ImmutableWeakReference object to avoid gc churn.
    private ImmutableWeakReference<Activity> mActivityWeakRefHolder;

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     *
     * @param context Context wrapping an activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ActivityWindowAndroid(
            Context context,
            boolean listenToActivityState,
            IntentRequestTracker intentRequestTracker) {
        this(
                context,
                listenToActivityState,
                new ActivityAndroidPermissionDelegate(
                        new WeakReference<Activity>(ContextUtils.activityFromContext(context))),
                new ActivityKeyboardVisibilityDelegate(
                        new WeakReference<Activity>(ContextUtils.activityFromContext(context))),
                intentRequestTracker);
    }

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     *
     * @param context Context wrapping an activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     * @param keyboardVisibilityDelegate Delegate which handles keyboard visibility.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ActivityWindowAndroid(
            Context context,
            boolean listenToActivityState,
            @NonNull ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            IntentRequestTracker intentRequestTracker) {
        this(
                context,
                listenToActivityState,
                new ActivityAndroidPermissionDelegate(
                        new WeakReference<Activity>(ContextUtils.activityFromContext(context))),
                keyboardVisibilityDelegate,
                intentRequestTracker);
    }

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     *
     * @param context Context wrapping an activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     * @param activityAndroidPermissionDelegate Delegates which handles android permissions.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    private ActivityWindowAndroid(
            Context context,
            boolean listenToActivityState,
            ActivityAndroidPermissionDelegate activityAndroidPermissionDelegate,
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
            ApplicationStatus.registerWindowFocusChangedListener(this);
        }

        activityKeyboardVisibilityDelegate.setLazyKeyboardInsetSupplier(
                LazyOneshotSupplier.fromSupplier(
                        () -> {
                            // `getInsetObserver()` implicitly checks for a window and for the
                            // activity to not be finishing.
                            var insetObserver = getInsetObserver();
                            if (insetObserver == null) {
                                // An InsetObserver can no longer be created. Stub this out so
                                // calls continue to succeed.
                                return new ObservableSupplierImpl<Integer>();
                            }
                            return insetObserver.getSupplierForKeyboardInset();
                        }));
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
            mActivityWeakRefHolder =
                    new ImmutableWeakReference<>(
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
            ApplicationStatus.unregisterWindowFocusChangedListener(this);
        }
    }

    @Override
    @ActivityState
    public int getActivityState() {
        return mListenToActivityState
                ? ApplicationStatus.getStateForActivity(getActivity().get())
                : super.getActivityState();
    }

    @Override
    public void onWindowFocusChanged(Activity activity, boolean hasFocus) {
        if (getActivity().get() == activity) onWindowFocusChanged(hasFocus);
    }
}

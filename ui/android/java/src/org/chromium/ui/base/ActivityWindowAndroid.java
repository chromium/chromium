// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;

import java.lang.ref.WeakReference;

/**
 * The class provides the WindowAndroid's implementation which requires Activity Instance. Only
 * instantiate this class when you need the implemented features.
 */
@NullMarked
public class ActivityWindowAndroid extends WindowAndroid
        implements ApplicationStatus.ActivityStateListener,
                ApplicationStatus.WindowFocusChangedListener {
    private final boolean mListenToActivityState;

    // Just create one ImmutableWeakReference object to avoid gc churn.
    private @Nullable ImmutableWeakReference<Activity> mActivityWeakRefHolder;

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     *
     * @param activity The activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     * @param insetObserver Observes window insets to track keyboard and layout changes.
     * @param trackOcclusion Whether to track occlusion of the window.
     */
    public static ActivityWindowAndroid create(
            Activity activity,
            boolean listenToActivityState,
            IntentRequestTracker intentRequestTracker,
            @Nullable InsetObserver insetObserver,
            boolean trackOcclusion) {
        return new ActivityWindowAndroid(
                activity,
                listenToActivityState,
                new ActivityAndroidPermissionDelegate(new WeakReference<>(activity)),
                new ActivityKeyboardVisibilityDelegate(new WeakReference<>(activity)),
                /* activityTopResumedSupported= */ false,
                intentRequestTracker,
                insetObserver,
                trackOcclusion);
    }

    /**
     * Creates an Activity-specific WindowAndroid with associated intent functionality.
     *
     * @param activity The activity associated with the WindowAndroid.
     * @param listenToActivityState Whether to listen to activity state changes.
     * @param activityAndroidPermissionDelegate Delegates which handles android permissions.
     * @param activityKeyboardVisibilityDelegate Delegate to handle keyboard visibility.
     * @param activityTopResumedSupported If true, allows the activity to report top resumed state
     *     changes.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     * @param insetObserver Observes window insets to track keyboard and layout changes.
     * @param trackOcclusion Whether to track occlusion of the window.
     */
    public ActivityWindowAndroid(
            Activity activity,
            boolean listenToActivityState,
            ActivityAndroidPermissionDelegate activityAndroidPermissionDelegate,
            ActivityKeyboardVisibilityDelegate activityKeyboardVisibilityDelegate,
            boolean activityTopResumedSupported,
            IntentRequestTracker intentRequestTracker,
            @Nullable InsetObserver insetObserver,
            boolean trackOcclusion) {
        super(
                activity,
                activityTopResumedSupported,
                intentRequestTracker,
                insetObserver,
                trackOcclusion);
        mListenToActivityState = listenToActivityState;
        if (listenToActivityState) {
            ApplicationStatus.registerStateListenerForActivity(this, activity);
            ApplicationStatus.registerWindowFocusChangedListener(this);
        }

        activityKeyboardVisibilityDelegate.setLazyKeyboardInsetSupplier(
                LazyOneshotSupplier.fromSupplier(
                        () -> {
                            if (insetObserver == null) {
                                // An InsetObserver can no longer be created. Stub this out so
                                // calls continue to succeed.
                                return ObservableSuppliers.alwaysNull();
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

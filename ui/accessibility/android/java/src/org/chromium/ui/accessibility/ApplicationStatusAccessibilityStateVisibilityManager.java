// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Implements {@link AccessibilityStateVisibilityManager} using {@link ApplicationStatus}. */
@NullMarked
public class ApplicationStatusAccessibilityStateVisibilityManager
        implements AccessibilityStateVisibilityManager {
    private final ApplicationStatus.ActivityStateListener mActivityStateListener =
            this::onActivityStateChange;
    private final ApplicationStatus.ApplicationStateListener mApplicationStateListener =
            this::onApplicationStateChange;

    private @Nullable Observer mObserver;

    @Override
    public void setObserver(@Nullable Observer observer) {
        if (mObserver != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            ApplicationStatus.unregisterApplicationStateListener(mApplicationStateListener);
        }
        mObserver = observer;
        if (mObserver != null) {
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
            ApplicationStatus.registerApplicationStateListener(mApplicationStateListener);
        }
    }

    private void onActivityStateChange(Activity activity, int newState) {
        if (mObserver == null) {
            return;
        }

        if (newState == ActivityState.RESUMED) {
            mObserver.onAnyActivityMadeVisible();
        }
    }

    private void onApplicationStateChange(int newState) {
        if (mObserver == null) {
            return;
        }

        if (newState != ApplicationState.HAS_RUNNING_ACTIVITIES
                && newState != ApplicationState.HAS_PAUSED_ACTIVITIES) {
            mObserver.onApplicationBackgrounded();
        } else if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            mObserver.onApplicationForegrounded();
        }
    }
}

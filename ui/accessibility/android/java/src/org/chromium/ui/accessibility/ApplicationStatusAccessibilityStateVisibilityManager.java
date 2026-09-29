// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Implements {@link AccessibilityStateVisibilityManager} using {@link ApplicationStatus}. */
@NullMarked
public class ApplicationStatusAccessibilityStateVisibilityManager
        implements AccessibilityStateVisibilityManager {
    private final ApplicationStatus.ActivityStateListener mActivityStateListener =
            this::onActivityStateChange;

    private @Nullable Observer mObserver;
    private boolean mHasVisibleActivities;

    @Override
    public void setObserver(@Nullable Observer observer) {
        if (mObserver != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
        }
        mObserver = observer;
        if (mObserver != null) {
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
            mHasVisibleActivities = ApplicationStatus.hasVisibleActivities();
        }
    }

    private void onActivityStateChange(Activity activity, int newState) {
        if (mObserver == null) {
            return;
        }

        if (newState == ActivityState.RESUMED) {
            mObserver.onActivityOrApplicationForegrounded();
        } else if (mHasVisibleActivities && !ApplicationStatus.hasVisibleActivities()) {
            mObserver.onApplicationBackgrounded();
        }
        mHasVisibleActivities = ApplicationStatus.hasVisibleActivities();
    }
}

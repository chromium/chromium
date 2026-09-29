// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ApplicationStatusAccessibilityStateVisibilityManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ApplicationStatusAccessibilityStateVisibilityManagerTest {
    @Mock private AccessibilityStateVisibilityManager.Observer mObserver;

    private ApplicationStatusAccessibilityStateVisibilityManager mVisibilityManager;
    private AutoCloseable mCloseableMocks;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        mVisibilityManager = new ApplicationStatusAccessibilityStateVisibilityManager();
    }

    @After
    public void tearDown() throws Exception {
        mVisibilityManager.setObserver(null);
        mCloseableMocks.close();
        ApplicationStatus.destroyForJUnitTests();
    }

    private Activity createActivity(@ActivityState int initialActivityState) {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        ApplicationStatus.onStateChangeForTesting(activity, initialActivityState);
        return activity;
    }

    @Test
    public void testSetObserver_nullObserverDoesNotReceiveCallbacks() {
        Activity activity = createActivity(ActivityState.PAUSED);

        mVisibilityManager.setObserver(mObserver);
        mVisibilityManager.setObserver(null);

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED);

        verifyNoInteractions(mObserver);
    }

    @Test
    public void testActivityLifecycle() {
        Activity activity = createActivity(ActivityState.STARTED);
        mVisibilityManager.setObserver(mObserver);

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.PAUSED);
        verify(mObserver, never()).onActivityOrApplicationForegrounded();
        verify(mObserver, never()).onApplicationBackgrounded();

        clearInvocations(mObserver);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED);
        verify(mObserver, never()).onActivityOrApplicationForegrounded();
        verify(mObserver).onApplicationBackgrounded();

        clearInvocations(mObserver);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        verify(mObserver).onActivityOrApplicationForegrounded();
        verify(mObserver, never()).onApplicationBackgrounded();
    }

    @Test
    public void testManagerCreatedWhenActivityStopped() {
        Activity activity = createActivity(ActivityState.STOPPED);
        mVisibilityManager.setObserver(mObserver);

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
        verify(mObserver, never()).onActivityOrApplicationForegrounded();
        verify(mObserver, never()).onApplicationBackgrounded();
    }

    @Test
    public void testOnActivityResumed_alreadyInForeground() {
        Activity activity1 = createActivity(ActivityState.STARTED);
        Activity activity2 = createActivity(ActivityState.PAUSED);
        mVisibilityManager.setObserver(mObserver);

        // Resume activity2
        ApplicationStatus.onStateChangeForTesting(activity2, ActivityState.RESUMED);
        verify(mObserver).onActivityOrApplicationForegrounded();
        verify(mObserver, never()).onApplicationBackgrounded();
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/** Tests for {@link ToastManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class ToastManagerTest {
    @Mock Toast mToast;
    @Mock Toast mToastNext;
    @Mock android.widget.Toast mAndroidToastObject;
    @Mock android.widget.Toast mAndroidToastObjectNext;

    private static final String TOAST_MSG = "now";
    private static final String TOAST_MSG_NEXT = "next";
    private static final long DURATION_BETWEEN_TOASTS_MS = 500;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @After
    public void tearDown() {
        waitForIdleUi();
        ToastManager.resetForTesting();
        mToast = null;
        clearInvocations(mAndroidToastObject);
        clearInvocations(mAndroidToastObjectNext);
    }

    private static void waitForIdleUi() {
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void showToast() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();

        ToastManager.getInstance().requestShow(mToast);
        verify(mAndroidToastObject).show();
    }

    @Test
    public void cancelCurrentToast() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();

        doReturn(Toast.ToastPriority.NORMAL).when(mToast).getPriority();
        doReturn(Toast.ToastPriority.NORMAL).when(mToastNext).getPriority();
        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG_NEXT).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();

        toastManager.requestShow(mToast);

        // Canceling lets the next queued one to show up immediately.
        toastManager.cancel(mToast);
        assertFalse("The current toast should have canceled", toastManager.isShowingForTesting());
        toastManager.requestShow(mToastNext);
        assertEquals(
                "The next toast should show right away",
                mToastNext,
                toastManager.getCurrentToast());
        verify(mAndroidToastObjectNext).show();
    }

    @Test
    public void cancelQueuedToast() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();

        doReturn(Toast.ToastPriority.NORMAL).when(mToast).getPriority();
        doReturn(Toast.ToastPriority.NORMAL).when(mToastNext).getPriority();
        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG_NEXT).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();

        toastManager.requestShow(mToast);
        toastManager.requestShow(mToastNext);

        // Canceling one in the queue does not affect visual change but
        // just removes the item from the queue, so won't show in the end.
        toastManager.cancel(mToastNext);
        assertEquals("Current toast should stay visible", mToast, toastManager.getCurrentToast());
        waitForIdleUi();
        verify(mAndroidToastObjectNext, never()).show();
    }

    @Test
    public void toastQueuedPriorityNormal() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();

        doReturn(Toast.ToastPriority.NORMAL).when(mToast).getPriority();
        doReturn(Toast.ToastPriority.NORMAL).when(mToastNext).getPriority();
        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG_NEXT).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();
        toastManager.requestShow(mToast);
        toastManager.requestShow(mToastNext);
        verify(mAndroidToastObjectNext, never()).show();

        // The next toast shows only after the delay.
        waitForIdleUi();
        ShadowLooper.idleMainLooper(DURATION_BETWEEN_TOASTS_MS, TimeUnit.MILLISECONDS);
        verify(mAndroidToastObjectNext).show();
    }

    @Test
    public void toastQueuedPriorityHigh() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();

        doReturn(Toast.ToastPriority.HIGH).when(mToast).getPriority();
        doReturn(Toast.ToastPriority.HIGH).when(mToastNext).getPriority();
        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG_NEXT).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();
        toastManager.requestShow(mToast);
        toastManager.requestShow(mToastNext);
        verify(mAndroidToastObjectNext, never()).show();

        // The next toast shows only after the delay.
        waitForIdleUi();
        ShadowLooper.idleMainLooper(DURATION_BETWEEN_TOASTS_MS, TimeUnit.MILLISECONDS);
        verify(mAndroidToastObjectNext).show();
    }

    @Test
    public void showHighPriorityToastAhead() {
        Toast toastNormal1 = mock(Toast.class);
        Toast toastNormal2 = mock(Toast.class);
        Toast toastHigh = mock(Toast.class);
        android.widget.Toast androidToast1 = mock(android.widget.Toast.class);
        android.widget.Toast androidToast2 = mock(android.widget.Toast.class);
        android.widget.Toast androidToast3 = mock(android.widget.Toast.class);
        final String toastMsgNormal1 = "normal1";
        final String toastMsgNormal2 = "normal2";
        final String toastMsgHigh = "high";

        doReturn(androidToast1).when(toastNormal1).getAndroidToast();
        doReturn(androidToast2).when(toastNormal2).getAndroidToast();
        doReturn(androidToast3).when(toastHigh).getAndroidToast();

        doReturn(Toast.ToastPriority.NORMAL).when(toastNormal1).getPriority();
        doReturn(Toast.ToastPriority.NORMAL).when(toastNormal2).getPriority();
        doReturn(Toast.ToastPriority.HIGH).when(toastHigh).getPriority();
        doReturn(toastMsgNormal1).when(toastNormal1).getText();
        doReturn(toastMsgNormal2).when(toastNormal2).getText();
        doReturn(toastMsgHigh).when(toastHigh).getText();

        ToastManager toastManager = ToastManager.getInstance();
        toastManager.requestShow(toastNormal1);
        toastManager.requestShow(toastNormal2);
        toastManager.requestShow(toastHigh);

        verify(androidToast1).show();
        waitForIdleUi();
        ShadowLooper.idleMainLooper(DURATION_BETWEEN_TOASTS_MS, TimeUnit.MILLISECONDS);
        verify(androidToast3).show(); // One with high priority comes before the next normal one.
        waitForIdleUi();
        ShadowLooper.idleMainLooper(DURATION_BETWEEN_TOASTS_MS, TimeUnit.MILLISECONDS);
        verify(androidToast2).show();
    }

    @Test
    public void discardDuplicatedToast() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();
        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();
        toastManager.requestShow(mToast);

        verify(mAndroidToastObject).show();
        clearInvocations(mAndroidToastObject);

        toastManager.requestShow(mToast);
        toastManager.requestShow(mToastNext);

        waitForIdleUi();
        verify(mAndroidToastObject, never()).show(); // Duplicated object

        waitForIdleUi();
        verify(mAndroidToastObjectNext, never()).show(); // Duplicated text content
    }

    @Test
    public void test500msGapBetweenTwoToasts() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();

        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG_NEXT).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();

        toastManager.requestShow(mToast);
        toastManager.requestShow(mToastNext);

        // The first toast should show without the 500ms delay.
        verify(mAndroidToastObject).show();

        // When the current toast is showing and the next toast is added to the queue,
        // the next toast should not show.
        assertEquals("mToast should be the current toast", mToast, toastManager.getCurrentToast());
        verify(mAndroidToastObjectNext, never()).show();

        waitForIdleUi();
        // When current toast is done showing but hasn't hit the 500ms gap in between shows,
        // the next toast should not show.
        assertEquals("mToast should be the current toast", mToast, toastManager.getCurrentToast());
        verify(mAndroidToastObjectNext, never()).show();

        // The next toast shows only after the current toast is done showing and the 500ms delay.
        ShadowLooper.idleMainLooper(DURATION_BETWEEN_TOASTS_MS, TimeUnit.MILLISECONDS);
        verify(mAndroidToastObjectNext).show();
    }

    @Test
    public void testNoUnnecessaryDelaysBetweenToasts() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();

        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG_NEXT).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();

        // The first toast should show without the 500ms delay.
        toastManager.requestShow(mToast);
        verify(mAndroidToastObject).show();
        waitForIdleUi();
        ShadowLooper.idleMainLooper(DURATION_BETWEEN_TOASTS_MS, TimeUnit.MILLISECONDS);
        // The second toast should also show without the 500ms delay.
        toastManager.requestShow(mToastNext);
        verify(mAndroidToastObjectNext).show();
    }

    @Test
    public void testCancelAndShowNextToast() {
        doReturn(mAndroidToastObject).when(mToast).getAndroidToast();
        doReturn(mAndroidToastObjectNext).when(mToastNext).getAndroidToast();

        doReturn(TOAST_MSG).when(mToast).getText();
        doReturn(TOAST_MSG_NEXT).when(mToastNext).getText();

        ToastManager toastManager = ToastManager.getInstance();

        toastManager.requestShow(mToast);
        toastManager.requestShow(mToastNext);

        verify(mAndroidToastObject).show();

        toastManager.cancel(mToast);
        assertFalse(
                "The current toast should have been canceled", toastManager.isShowingForTesting());
        // The next toast should not show immediately.
        verify(mAndroidToastObjectNext, never()).show();

        ShadowLooper.idleMainLooper(DURATION_BETWEEN_TOASTS_MS, TimeUnit.MILLISECONDS);
        // The next toast should show after the 500ms delay.
        verify(mAndroidToastObjectNext).show();
    }
}

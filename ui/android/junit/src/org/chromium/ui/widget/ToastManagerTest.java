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

import android.os.Build;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.util.concurrent.TimeUnit;

/** Tests for {@link ToastManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToastManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock Toast mToast;
    @Mock Toast mToastNext;
    @Mock android.widget.Toast mAndroidToastObject;
    @Mock android.widget.Toast mAndroidToastObjectNext;

    private static final String TOAST_MSG = "now";
    private static final String TOAST_MSG_NEXT = "next";
    private static final long DURATION_BETWEEN_TOASTS_MS = 500;
    private static final long DURATION_SHORT_MS = 2000;

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
        triggerCallback(mAndroidToastObject);
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
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
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
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        triggerCallback(mAndroidToastObject);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
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
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        triggerCallback(mAndroidToastObject);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
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
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        triggerCallback(androidToast1);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(androidToast3).show(); // One with high priority comes before the next normal one.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        triggerCallback(androidToast3);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
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

        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(mAndroidToastObject, never()).show(); // Duplicated object
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

        // When current toast is done showing but hasn't hit the 500ms gap in between shows,
        // the next toast should not show.
        triggerCallback(mAndroidToastObject);
        assertEquals("mToast should be the current toast", mToast, toastManager.getCurrentToast());
        verify(mAndroidToastObjectNext, never()).show();

        // The next toast shows only after the current toast is done showing and the 500ms delay.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
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
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        triggerCallback(mAndroidToastObject);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
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
        triggerCallback(mAndroidToastObject);
        assertFalse(
                "The current toast should have been canceled", toastManager.isShowingForTesting());
        // The next toast should not show immediately.
        verify(mAndroidToastObjectNext, never()).show();

        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        // The next toast should show after the 500ms delay.
        verify(mAndroidToastObjectNext).show();
    }

    private void triggerCallback(android.widget.Toast mockToast) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            ArgumentCaptor<android.widget.Toast.Callback> callbackCaptor =
                    ArgumentCaptor.forClass(android.widget.Toast.Callback.class);
            verify(mockToast).addCallback(callbackCaptor.capture());
            callbackCaptor.getValue().onToastHidden();
        } else if (ToastManager.getInstance().isShowingForTesting()) {
            ShadowLooper.idleMainLooper(DURATION_SHORT_MS, TimeUnit.MILLISECONDS);
        }
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/** Unit tests for {@link OverscrollRefreshHandlerHolder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OverscrollRefreshHandlerHolderUnitTest {
    private static final long NATIVE_PTR_1 = 1001L;
    private static final long NATIVE_PTR_2 = 2002L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OverscrollRefreshHandler mHandler1;
    @Mock private OverscrollRefreshHandler mHandler2;

    @Test
    public void testDispatchForwardsToRegisteredHandler() {
        when(mHandler1.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT))
                .thenReturn(true);
        when(mHandler1.start(OverscrollAction.HISTORY_NAVIGATION, BackGestureEventSwipeEdge.RIGHT))
                .thenReturn(false);

        OverscrollRefreshHandlerHolder.setRef(NATIVE_PTR_1, mHandler1);
        OverscrollRefreshHandlerHolder.setRef(NATIVE_PTR_2, mHandler2);
        try {
            assertTrue(
                    OverscrollRefreshHandlerHolder.onPullStart(
                            NATIVE_PTR_1,
                            OverscrollAction.PULL_TO_REFRESH,
                            BackGestureEventSwipeEdge.LEFT));
            assertFalse(
                    OverscrollRefreshHandlerHolder.onPullStart(
                            NATIVE_PTR_1,
                            OverscrollAction.HISTORY_NAVIGATION,
                            BackGestureEventSwipeEdge.RIGHT));
            OverscrollRefreshHandlerHolder.onPullUpdate(NATIVE_PTR_1, 1.5f, -2.5f);
            OverscrollRefreshHandlerHolder.onPullRelease(
                    NATIVE_PTR_1, OverscrollActivationStatus.ALLOW_ACTIVATION);
            OverscrollRefreshHandlerHolder.onPullReset(NATIVE_PTR_1);

            verify(mHandler1)
                    .start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT);
            verify(mHandler1)
                    .start(OverscrollAction.HISTORY_NAVIGATION, BackGestureEventSwipeEdge.RIGHT);
            verify(mHandler1).pull(1.5f, -2.5f);
            verify(mHandler1).release(OverscrollActivationStatus.ALLOW_ACTIVATION);
            verify(mHandler1).reset();
            verifyNoInteractions(mHandler2);
        } finally {
            OverscrollRefreshHandlerHolder.removeRef(NATIVE_PTR_1);
            OverscrollRefreshHandlerHolder.removeRef(NATIVE_PTR_2);
        }
    }

    @Test
    public void testDuplicateSetRefAsserts() {
        OverscrollRefreshHandlerHolder.setRef(NATIVE_PTR_1, mHandler1);
        try {
            assertThrows(
                    AssertionError.class,
                    () -> OverscrollRefreshHandlerHolder.setRef(NATIVE_PTR_1, mHandler2));
        } finally {
            OverscrollRefreshHandlerHolder.removeRef(NATIVE_PTR_1);
        }
    }

    @Test
    public void testRemoveMissingRefAsserts() {
        assertThrows(
                AssertionError.class, () -> OverscrollRefreshHandlerHolder.removeRef(NATIVE_PTR_1));
    }

    @Test
    public void testDispatchAfterRemoveRefAsserts() {
        OverscrollRefreshHandlerHolder.setRef(NATIVE_PTR_1, mHandler1);
        OverscrollRefreshHandlerHolder.removeRef(NATIVE_PTR_1);

        assertThrows(
                AssertionError.class,
                () -> OverscrollRefreshHandlerHolder.onPullReset(NATIVE_PTR_1));
    }
}

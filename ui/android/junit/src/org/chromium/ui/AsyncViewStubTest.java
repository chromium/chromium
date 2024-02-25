// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.shadows.ShadowAsyncLayoutInflater;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests logic in the AsyncViewStub class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAsyncLayoutInflater.class})
public class AsyncViewStubTest {
    private AsyncViewStub mAsyncViewStub;
    private final AtomicInteger mEventCount = new AtomicInteger();
    private static final int MAIN_LAYOUT_RESOURCE_ID = org.chromium.ui.R.layout.main_view;
    private static final int INFLATE_LAYOUT_RESOURCE_ID = org.chromium.ui.R.layout.inflated_view;
    private static final int STUB_ID = org.chromium.ui.R.id.view_stub;

    @Before
    public void setUp() {
        LinearLayout mainView =
                (LinearLayout)
                        LayoutInflater.from(RuntimeEnvironment.application)
                                .inflate(MAIN_LAYOUT_RESOURCE_ID, null);
        mAsyncViewStub = mainView.findViewById(STUB_ID);
        mAsyncViewStub.setLayoutResource(INFLATE_LAYOUT_RESOURCE_ID);
        mAsyncViewStub.setShouldInflateOnBackgroundThread(true);
        mEventCount.set(0);
    }

    @Test
    public void testCallsListenersOnUiThread() {
        mAsyncViewStub.addOnInflateListener(
                (View v) -> {
                    assertTrue(ThreadUtils.runningOnUiThread());
                    mEventCount.incrementAndGet();
                });
        mAsyncViewStub.inflate();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        // ensure callback gets called.
        assertEquals(mEventCount.get(), 1);
    }

    @Test
    public void testCallsListenersInOrder() {
        mAsyncViewStub.addOnInflateListener(
                (View v) -> {
                    assertEquals(mEventCount.incrementAndGet(), 1);
                });
        mAsyncViewStub.addOnInflateListener(
                (View v) -> {
                    assertEquals(mEventCount.incrementAndGet(), 2);
                });
        mAsyncViewStub.addOnInflateListener(
                (View v) -> {
                    assertEquals(mEventCount.decrementAndGet(), 1);
                });
        assertEquals(mEventCount.get(), 0);
        mAsyncViewStub.inflate();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(mEventCount.get(), 1);
    }
}

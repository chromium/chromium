// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
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

/** Tests logic in the AsyncViewProvider class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAsyncLayoutInflater.class})
public class AsyncViewProviderTest {
    private LinearLayout mRoot;
    private AsyncViewStub mAsyncViewStub;
    private AsyncViewProvider<View> mAsyncViewProvider;
    private final AtomicInteger mEventCount = new AtomicInteger();
    private static final int MAIN_LAYOUT_RESOURCE_ID = R.layout.main_view;
    private static final int INFLATE_LAYOUT_RESOURCE_ID = R.layout.inflated_view;
    private static final int STUB_ID = R.id.view_stub;
    private static final int INFLATED_VIEW_ID = R.id.inflated_view;
    private static final int PREINFLATED_VIEW_ID = R.id.pre_inflated_view;

    @Before
    public void setUp() {
        mRoot =
                (LinearLayout)
                        LayoutInflater.from(RuntimeEnvironment.application)
                                .inflate(MAIN_LAYOUT_RESOURCE_ID, null);
        mAsyncViewStub = mRoot.findViewById(STUB_ID);
        mAsyncViewStub.setLayoutResource(INFLATE_LAYOUT_RESOURCE_ID);
        mAsyncViewStub.setShouldInflateOnBackgroundThread(true);
        mAsyncViewProvider = AsyncViewProvider.of(mAsyncViewStub, INFLATED_VIEW_ID);
        mAsyncViewStub.setId(STUB_ID);
        mEventCount.set(0);
    }

    @Test
    public void testCreatesUnloadedProviderIfNotInflated() {
        AsyncViewProvider provider = AsyncViewProvider.of(mAsyncViewStub, INFLATED_VIEW_ID);
        assertNull(provider.get());
    }

    @Test
    public void testCreatesLoadedProviderIfInflated() {
        mAsyncViewStub.inflate();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        AsyncViewProvider provider = AsyncViewProvider.of(mAsyncViewStub, INFLATED_VIEW_ID);
        assertNotNull(provider.get());
    }

    @Test
    public void testCreatesUnloadedProviderUsingResourceIds() {
        AsyncViewProvider provider = AsyncViewProvider.of(mRoot, STUB_ID, INFLATED_VIEW_ID);
        assertNull(provider.get());
    }

    @Test
    public void testCreatesLoadedProviderUsingResourceIds() {
        mAsyncViewStub.inflate();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        AsyncViewProvider provider = AsyncViewProvider.of(mRoot, STUB_ID, INFLATED_VIEW_ID);
        assertNotNull(provider.get());
    }

    @Test
    public void testCreatesLoadedProviderUsingResourceIdsWithoutAsyncViewStub() {
        AsyncViewProvider provider = AsyncViewProvider.of(mRoot, 0, PREINFLATED_VIEW_ID);
        assertNotNull(provider.get());
        assertTrue(provider.get() instanceof ImageView);
    }

    @Test
    public void testRunsCallbackImmediatelyIfLoaded() {
        mAsyncViewStub.inflate();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        AsyncViewProvider<View> provider = AsyncViewProvider.of(mAsyncViewStub, INFLATED_VIEW_ID);
        assertEquals(mEventCount.get(), 0);
        provider.whenLoaded(
                (View v) -> {
                    mEventCount.incrementAndGet();
                });
        assertEquals(mEventCount.get(), 1);
        provider.whenLoaded(
                (View v) -> {
                    mEventCount.incrementAndGet();
                });
        assertEquals(mEventCount.get(), 2);
    }

    @Test
    public void testCallsListenersOnUiThread() {
        mAsyncViewProvider.whenLoaded(
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
        mAsyncViewProvider.whenLoaded(
                (View v) -> {
                    assertEquals(mEventCount.incrementAndGet(), 1);
                });
        mAsyncViewProvider.whenLoaded(
                (View v) -> {
                    assertEquals(mEventCount.incrementAndGet(), 2);
                });
        mAsyncViewProvider.whenLoaded(
                (View v) -> {
                    assertEquals(mEventCount.decrementAndGet(), 1);
                });
        assertEquals(mEventCount.get(), 0);
        mAsyncViewStub.inflate();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(mEventCount.get(), 1);
    }
}

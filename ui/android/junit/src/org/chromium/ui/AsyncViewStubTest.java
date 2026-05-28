// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.build.annotations.NullMarked;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests logic in the AsyncViewStub class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@NullMarked
public class AsyncViewStubTest {
    private LinearLayout mMainView;
    private AsyncViewStub mAsyncViewStub;
    private final AtomicInteger mEventCount = new AtomicInteger();
    private static final @LayoutRes int MAIN_LAYOUT_RESOURCE_ID = R.layout.main_view;
    private static final @LayoutRes int MAIN_WITH_INFLATED_ID_LAYOUT_RESOURCE_ID =
            R.layout.main_view_with_inflated_id;
    private static final @LayoutRes int INFLATE_LAYOUT_RESOURCE_ID = R.layout.inflated_view;
    private static final @IdRes int STUB_ID = R.id.view_stub;
    private static final @IdRes int STUB_WITH_INFLATED_ID_ID = R.id.view_stub_with_inflated_id;
    private static final @IdRes int INFLATED_ID_FROM_XML = R.id.inflated_id_from_xml;

    @Before
    public void setUp() {
        mMainView =
                (LinearLayout)
                        LayoutInflater.from(RuntimeEnvironment.application)
                                .inflate(MAIN_LAYOUT_RESOURCE_ID, null);
        mAsyncViewStub = mMainView.findViewById(STUB_ID);
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
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        // ensure callback gets called.
        assertEquals(1, mEventCount.get());
    }

    @Test
    public void testCallsListenersInOrder() {
        mAsyncViewStub.addOnInflateListener(
                (View v) -> {
                    assertEquals(1, mEventCount.incrementAndGet());
                });
        mAsyncViewStub.addOnInflateListener(
                (View v) -> {
                    assertEquals(2, mEventCount.incrementAndGet());
                });
        mAsyncViewStub.addOnInflateListener(
                (View v) -> {
                    assertEquals(1, mEventCount.decrementAndGet());
                });
        assertEquals(0, mEventCount.get());
        mAsyncViewStub.inflate();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(1, mEventCount.get());
    }

    @Test
    public void testInflatedIdProgrammatic() {
        int inflatedId = View.generateViewId();
        mAsyncViewStub.setInflatedId(inflatedId);
        assertEquals(inflatedId, mAsyncViewStub.getInflatedId());

        mAsyncViewStub.inflate();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        View inflatedView = mMainView.findViewById(inflatedId);
        assertNotNull(inflatedView);
        assertEquals(mAsyncViewStub.getInflatedView(), inflatedView);
    }

    @Test
    public void testInflatedIdFromXml() {
        LinearLayout mainView =
                (LinearLayout)
                        LayoutInflater.from(RuntimeEnvironment.application)
                                .inflate(MAIN_WITH_INFLATED_ID_LAYOUT_RESOURCE_ID, null);
        AsyncViewStub asyncViewStub = mainView.findViewById(STUB_WITH_INFLATED_ID_ID);
        asyncViewStub.setLayoutResource(INFLATE_LAYOUT_RESOURCE_ID);
        asyncViewStub.setShouldInflateOnBackgroundThread(true);

        assertEquals(INFLATED_ID_FROM_XML, asyncViewStub.getInflatedId());

        asyncViewStub.inflate();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        View inflatedView = mainView.findViewById(INFLATED_ID_FROM_XML);
        assertNotNull(inflatedView);
        assertEquals(asyncViewStub.getInflatedView(), inflatedView);
    }
}

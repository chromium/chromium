// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.graphics.Point;
import android.os.SystemClock;
import android.util.Range;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.BrowserControlsOffsetCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.List;

/**
 * Test for ScrollOffsetCallback.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@CommandLineFlags.Add("enable-features=ImmediatelyHideBrowserControlsForTest")
@DisabledTest(message = "https://crbug.com/1315399")
public class BrowserControlsOffsetCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final class BrowserControlsOffsetCallbackImpl
            extends BrowserControlsOffsetCallback {
        // All offsets are added to either |mTopOffsets| or |mBottomOffsets|.
        public List<Integer> mTopOffsets = new ArrayList<>();
        public List<Integer> mBottomOffsets = new ArrayList<>();
        public CallbackHelper mCallbackHelper = new CallbackHelper();
        // If non-null an offset and an offset lies in the specified range
        // mCallbackHelper.notifyCalled() is invoked.
        public Range<Integer> mTopTriggerRange;
        public Range<Integer> mBottomTriggerRange;

        @Override
        public void onTopViewOffsetChanged(int offset) {
            mTopOffsets.add(offset);
            if (mTopTriggerRange != null && mTopTriggerRange.contains(offset)) {
                mTopTriggerRange = null;
                mCallbackHelper.notifyCalled();
            }
        }

        @Override
        public void onBottomViewOffsetChanged(int offset) {
            mBottomOffsets.add(offset);
            if (mBottomTriggerRange != null && mBottomTriggerRange.contains(offset)) {
                mBottomTriggerRange = null;
                mCallbackHelper.notifyCalled();
            }
        }
    }

    private BrowserControlsOffsetCallbackImpl mBrowserControlsOffsetCallback =
            new BrowserControlsOffsetCallbackImpl();

    private BrowserControlsHelper mBrowserControlsHelper;
    private Point mCurrentDragPoint;
    private boolean mInDrag;

    @Before
    public void setUp() throws Throwable {
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().registerBrowserControlsOffsetCallback(
                    mBrowserControlsOffsetCallback);
        });

        mBrowserControlsHelper =
                BrowserControlsHelper.createAndBlockUntilBrowserControlsInitializedInSetUp(
                        activity);
    }

    private void startDrag() {
        View view = mActivityTestRule.getActivity().getWindow().getDecorView();
        Assert.assertFalse(mInDrag);
        mInDrag = true;
        view.post(() -> {
            mCurrentDragPoint = new Point(view.getWidth() / 2, view.getHeight() / 2);
            long eventTime = SystemClock.uptimeMillis();
            view.dispatchTouchEvent(MotionEvent.obtain(eventTime, eventTime,
                    MotionEvent.ACTION_DOWN, mCurrentDragPoint.x, mCurrentDragPoint.y, 0));
        });
    }

    private void dragBy(final int deltaY) {
        View view = mActivityTestRule.getActivity().getWindow().getDecorView();
        Assert.assertTrue(mInDrag);
        view.post(() -> {
            long eventTime = SystemClock.uptimeMillis();
            mCurrentDragPoint.y += deltaY;
            view.dispatchTouchEvent(MotionEvent.obtain(eventTime, eventTime,
                    MotionEvent.ACTION_MOVE, mCurrentDragPoint.x, mCurrentDragPoint.y, 0));
        });
    }

    @Test
    @SmallTest
    public void testTopScroll() throws Exception {
        int topViewHeight = mBrowserControlsHelper.getTopViewHeight();
        CallbackHelper callbackHelper = mBrowserControlsOffsetCallback.mCallbackHelper;

        // Scroll half the height.
        mBrowserControlsOffsetCallback.mTopTriggerRange = new Range(-topViewHeight + 2, -2);
        startDrag();
        dragBy(-topViewHeight / 2);
        int callCount = 0;
        callbackHelper.waitForCallback(callCount++);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBrowserControlsOffsetCallback.mTopOffsets.clear();
            mBrowserControlsOffsetCallback.mTopTriggerRange =
                    new Range(-topViewHeight, -topViewHeight);
        });

        // Scroll a lot to ensure top view completely hides.
        dragBy(-topViewHeight);
        callbackHelper.waitForCallback(callCount++);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBrowserControlsOffsetCallback.mTopOffsets.clear();
            mBrowserControlsOffsetCallback.mTopTriggerRange = new Range(-topViewHeight + 2, -2);
        });

        // Scroll up half the height to trigger showing again.
        dragBy(topViewHeight / 2);
        callbackHelper.waitForCallback(callCount++);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBrowserControlsOffsetCallback.mTopOffsets.clear();
            mBrowserControlsOffsetCallback.mTopTriggerRange = new Range(0, 0);
        });

        // And enough to be fully visible.
        dragBy(topViewHeight);
        callbackHelper.waitForCallback(callCount++);
    }

    @Test
    @SmallTest
    public void testBottomScroll() throws Exception {
        CallbackHelper callbackHelper = mBrowserControlsOffsetCallback.mCallbackHelper;
        int topViewHeight = mBrowserControlsHelper.getTopViewHeight();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(bottomView);

        int bottomViewHeight =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return bottomView.getHeight(); });
        Assert.assertTrue(bottomViewHeight > 0);
        // The amount necessary to scroll is the sum of the two views. This is because the page
        // height is reduced by the sum of these two.
        int maxViewsHeight = topViewHeight + bottomViewHeight;

        // Wait for cc to see the bottom height. This is very important, as scrolling is gated by
        // cc getting the bottom height.
        mBrowserControlsHelper.waitForBrowserControlsMetadataState(topViewHeight, bottomViewHeight);

        // Move by the size of the controls, which should hide both.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBrowserControlsOffsetCallback.mTopTriggerRange =
                    new Range(-topViewHeight, -topViewHeight);
            mBrowserControlsOffsetCallback.mBottomTriggerRange =
                    new Range(bottomViewHeight, bottomViewHeight);
        });
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -maxViewsHeight);
        int callCount = 0;
        // 2 is for the top and bottom.
        callbackHelper.waitForCallback(callCount, 2);
        callCount += 2;

        // Move so top and bottom controls are shown again.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBrowserControlsOffsetCallback.mTopTriggerRange = new Range(0, 0);
            mBrowserControlsOffsetCallback.mBottomTriggerRange = new Range(0, 0);
        });
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, maxViewsHeight);
        // 2 is for the top and bottom.
        callbackHelper.waitForCallback(callCount, 2);
        callCount += 2;
    }
}

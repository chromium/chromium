// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.view.DragEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.util.Pair;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.dragdrop.DragEventDispatchHelper.DragEventDispatchDestination;

/** Unit test for {@link DragEventDispatchHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowView.class)
public class DragEventDispatchHelperUnitTest {
    private static final int VIEW_SIZE = 100;

    DragEventDispatchDestination mDestination;
    DragEventDispatchHelper mHelper;

    Activity mActivity;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule().silent();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    FrameLayout mContentView;
    View mDestinationView;
    View mStarterView;

    PayloadCallbackHelper<Pair<Integer, Integer>> mCordCallbackHelper =
            new PayloadCallbackHelper<>();
    PayloadCallbackHelper<DragEvent> mDragEventCallbackHelper = new PayloadCallbackHelper<>();

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mActivityScenario.getScenario().moveToState(State.STARTED);

        mContentView = new FrameLayout(mActivity);
        mActivity.setContentView(mContentView);

        mStarterView = new View(mActivity);
        mDestinationView = new View(mActivity);
        mDestinationView.setOnDragListener(
                (view, dragEvent) -> {
                    mDragEventCallbackHelper.notifyCalled(dragEvent);
                    return true;
                });

        mContentView.addView(mDestinationView);
        mContentView.addView(mStarterView);
        mStarterView.bringToFront();

        mDestination =
                new DragEventDispatchDestination() {
                    @Override
                    public View view() {
                        return mDestinationView;
                    }

                    @Override
                    public boolean onDragEventWithOffset(DragEvent event, int dx, int dy) {
                        mCordCallbackHelper.notifyCalled(new Pair<>(dx, dy));
                        return mDestinationView.dispatchDragEvent(event);
                    }
                };
        mHelper = new DragEventDispatchHelper(mStarterView, mDestination);
    }

    @Test
    public void supportActions() {
        int[] defaultSupportedDragActions =
                new int[] {
                    DragEvent.ACTION_DRAG_LOCATION,
                    DragEvent.ACTION_DROP,
                    DragEvent.ACTION_DRAG_ENTERED,
                    DragEvent.ACTION_DRAG_EXITED,
                };

        int[] defaultUnSupportedActions =
                new int[] {
                    DragEvent.ACTION_DRAG_STARTED, DragEvent.ACTION_DRAG_ENDED,
                };

        for (int action : defaultSupportedDragActions) {
            assertTrue("Default for supported action is wrong.", mHelper.isActionSupported(action));
        }

        for (int action : defaultUnSupportedActions) {
            assertFalse(
                    "Default for unsupported action is wrong.", mHelper.isActionSupported(action));
        }

        mHelper.markActionSupported(DragEvent.ACTION_DRAG_LOCATION, false);
        assertFalse(
                "Removed action is no longer supported.",
                mHelper.isActionSupported(DragEvent.ACTION_DRAG_LOCATION));
        mHelper.markActionSupported(DragEvent.ACTION_DROP, false);
        assertFalse(
                "Removed action is no longer supported.",
                mHelper.isActionSupported(DragEvent.ACTION_DROP));

        mHelper.markActionSupported(DragEvent.ACTION_DRAG_LOCATION, true);
        assertTrue(
                "Action is supported again.",
                mHelper.isActionSupported(DragEvent.ACTION_DRAG_LOCATION));
        mHelper.markActionSupported(DragEvent.ACTION_DROP, true);
        assertTrue("Action is supported again.", mHelper.isActionSupported(DragEvent.ACTION_DROP));
    }

    @Test
    public void alwaysAcceptDragStart() {
        DragEvent d1 = mockDragEvent(DragEvent.ACTION_DRAG_STARTED, 1f, 1f);
        assertTrue("Drag start is always handled by #onDrag", mHelper.onDrag(mStarterView, d1));
    }

    @Test
    public void dispatchDragWithOffset() {
        // As start, configure the 2 views to be the same location, and start view sit on top.
        configureScreenLocation(mStarterView, 0, 0, VIEW_SIZE);
        configureScreenLocation(mDestinationView, 0, 0, VIEW_SIZE);
        mStarterView.bringToFront();

        // No offset expected for views starting at the same location.
        DragEvent d1 = mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, 1f, 1f);
        mStarterView.dispatchDragEvent(d1);
        verifyDestination(d1, 0, 0);

        configureScreenLocation(mStarterView, 50, 0, VIEW_SIZE);
        DragEvent d2 = mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, 10f, 10f);
        mStarterView.dispatchDragEvent(d2);
        verifyDestination(d2, 50, 0);

        // Enter does not have a offset.
        DragEvent d3 = mockDragEvent(DragEvent.ACTION_DRAG_EXITED, 1f, 1f);
        mStarterView.dispatchDragEvent(d3);
        verifyDestination(d3, 0, 0);

        // Test another set of offset.
        configureScreenLocation(mDestinationView, 50, 50, VIEW_SIZE);
        DragEvent d4 = mockDragEvent(DragEvent.ACTION_DROP, 10f, 10f);
        mStarterView.dispatchDragEvent(d4);
        verifyDestination(d4, 0, -50);
    }

    @Test
    public void doNotDispatch_DestinationDisabled() {
        mDestinationView.setEnabled(false);

        DragEvent d1 = mockDragEvent(DragEvent.ACTION_DRAG_STARTED, 1f, 1f);
        mStarterView.dispatchDragEvent(d1);
        assertEquals(
                "Should not receive dispatched view when destination is disabled.",
                0,
                mDragEventCallbackHelper.getCallCount());
    }

    @Test
    public void doNotDispatch_DestinationNotAttached() {
        mContentView.removeView(mDestinationView);
        assertFalse(mDestinationView.isAttachedToWindow());

        DragEvent d1 = mockDragEvent(DragEvent.ACTION_DRAG_STARTED, 1f, 1f);
        mStarterView.dispatchDragEvent(d1);
        assertEquals(
                "Should not receive dispatched view when destination is not attached.",
                0,
                mDragEventCallbackHelper.getCallCount());
    }

    private void verifyDestination(DragEvent expectedEvent, int expectedDx, int expectedDy) {
        assertEquals(mDragEventCallbackHelper.getCallCount(), mCordCallbackHelper.getCallCount());

        int counts = mCordCallbackHelper.getCallCount();
        Pair<Integer, Integer> pair = mCordCallbackHelper.getPayloadByIndexBlocking(counts - 1);
        DragEvent dragEvent = mDragEventCallbackHelper.getPayloadByIndexBlocking(counts - 1);

        assertEquals("DragEvent passed is different.", expectedEvent, dragEvent);
        assertEquals("Forwarded X offset is different.", expectedDx, pair.first.intValue());
        assertEquals("Forwarded Y offset is different.", expectedDy, pair.second.intValue());
    }

    private static DragEvent mockDragEvent(int action, float x, float y) {
        DragEvent event = Mockito.mock(DragEvent.class);
        doReturn(action).when(event).getAction();
        doReturn(x).when(event).getX();
        doReturn(y).when(event).getY();
        return event;
    }

    private static void configureScreenLocation(View view, int x, int y, int size) {
        view.layout(x, y, x + size, y + size);
    }
}

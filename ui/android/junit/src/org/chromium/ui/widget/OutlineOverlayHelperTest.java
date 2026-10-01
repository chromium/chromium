// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;

import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link OutlineOverlayHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OutlineOverlayHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private View mHost;
    private FrameLayout mParent;
    private Drawable mOutlineDrawable;

    private TestActivity mActivity;
    private OutlineOverlayHelper mHelper;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);

        mParent = new FrameLayout(mActivity);
        mHost = new View(mActivity);
        mHost.setFocusable(true);
        mHost.setFocusableInTouchMode(true);
        mParent.addView(mHost);
        mActivity.setContentView(mParent);

        // Spy on the real drawable to verify interactions without mocking its behavior.
        Drawable realDrawable =
                mActivity.getDrawable(R.drawable.focused_outline_overlay_corners_16dp);
        mOutlineDrawable = spy(realDrawable);

        mHelper = new OutlineOverlayHelper(mHost, mParent, mOutlineDrawable);
    }

    @Test
    public void testInitialState_NoFocus() {
        // The helper's constructor posts an update. Run pending tasks to trigger it.
        mHelper.onFocusChange(mHost, false);

        // Verify that the overlay is not added when the view doesn't have focus.
        assertFalse(
                "Outline should not be visible when host view does not have focus.",
                mHelper.isOutlineAttachedForTesting());
    }

    @Test
    public void testFocusGain() {
        mHost.requestFocus();

        // Verify that the overlay is added when the view gains focus.
        assertTrue(
                "Outline should be visible when host view has focus.",
                mHelper.isOutlineAttachedForTesting());
    }

    @Test
    public void testFocusLoss() {
        // First, gain focus.
        mHost.requestFocus();
        assertTrue(
                "Outline should be visible when host view has focus.",
                mHelper.isOutlineAttachedForTesting());

        // Then, lose focus by moving it to another view.
        View otherView = new View(mActivity);
        otherView.setFocusable(true);
        otherView.setFocusableInTouchMode(true);
        mParent.addView(otherView);
        otherView.requestFocus();

        // Verify that the overlay is removed when the view loses focus.
        assertFalse(
                "Outline should not be visible when host view does not have focus.",
                mHelper.isOutlineAttachedForTesting());
    }

    @Test
    public void testLayoutChange_UpdatesBounds() {
        // Gain focus to show the outline.
        mHost.requestFocus();

        // Simulate a layout change.
        int left = 10;
        int top = 20;
        int right = 110;
        int bottom = 120;
        mHost.layout(left, top, right, bottom);
        // Robolectric test does not automatically trigger onLayoutChangeListener.
        mHelper.onLayoutChange(mHost, left, top, right, bottom, 0, 0, 0, 0);

        // Verify the drawable's bounds are updated correctly.
        int offset = mActivity.getResources().getDimensionPixelSize(R.dimen.focused_outline_offset);
        Rect expectedBounds =
                new Rect(left - offset, top - offset, right + offset, bottom + offset);
        assertEquals(
                "Outline bounds should be updated after layout change.",
                expectedBounds,
                mOutlineDrawable.getBounds());
    }

    @Test
    public void testDestroy() {
        // Gain focus to show the outline.
        mHost.requestFocus();
        assertTrue(
                "Outline should be visible when host view has focus.",
                mHelper.isOutlineAttachedForTesting());

        assertNotNull(mOutlineDrawable.getCallback());

        mHelper.destroy();

        assertNull(mHost.getOnFocusChangeListener());
        // The outline drawable should have been removed from the parent overlay.
        assertNull(mOutlineDrawable.getCallback());
    }

    @Test
    public void testAttach() {
        // Detach the host from the parent to test the attach functionality.
        mParent.removeView(mHost);

        OutlineOverlayHelper.attach(mHost, mOutlineDrawable);
        assertFalse(
                "Outline should not be visible when host view is not attached.",
                mHelper.isOutlineAttachedForTesting());

        // Re-attach the host to the parent. This should trigger the creation of the helper.
        mParent.addView(mHost);
        mHost.requestFocus();
        mHelper.onFocusChange(mHost, true);
        assertTrue(
                "Outline should be visible when host view is attached and has focus.",
                mHelper.isOutlineAttachedForTesting());
    }
}

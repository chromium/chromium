// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;

import android.graphics.RectF;
import android.view.DragEvent;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the EventOffsetHandler. */
@RunWith(BaseRobolectricTestRunner.class)
public class EventOffsetHandlerTest {
    private EventOffsetHandler mHandler;
    private final EventOffsetHandler.EventOffsetHandlerDelegate mDelegate =
            new EventOffsetHandler.EventOffsetHandlerDelegate() {
                @Override
                public float getLeft() {
                    return mViewport.left;
                }

                @Override
                public float getTop() {
                    return mViewport.top;
                }

                @Override
                public void setCurrentTouchEventOffsets(float left, float top) {
                    mOffsetX = left;
                    mOffsetY = top;
                }

                @Override
                public void setCurrentDragEventOffsets(float dx, float dy) {
                    mDragOffsetX = dx;
                    mDragOffsetY = dy;
                }
            };

    private RectF mViewport;
    private float mOffsetX;
    private float mOffsetY;
    private float mDragOffsetX;
    private float mDragOffsetY;

    private void assertOffsets(float x, float y) {
        assertEquals(x, mOffsetX, 0.0);
        assertEquals(y, mOffsetY, 0.0);
    }

    private void assertDragOffsets(float dx, float dy) {
        assertEquals(dx, mDragOffsetX, 0.0);
        assertEquals(dy, mDragOffsetY, 0.0);
    }

    @Before
    public void setUp() {
        mHandler = new EventOffsetHandler(mDelegate);
        mViewport = new RectF(100, 200, 600, 800);
        assertOffsets(0, 0);
        assertDragOffsets(0, 0);
    }

    @Test
    public void testOffsetChangesWhileDragging() {
        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_STARTED, 0.f, 0.f);
        mHandler.onPostDispatchDragEvent(DragEvent.ACTION_DRAG_STARTED);

        // Viewport position has been negated.
        assertOffsets(-100, -200);

        MotionEvent motionStart = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        mHandler.onInterceptTouchDownEvent(motionStart);

        assertOffsets(-100, -200);

        MotionEvent motionEnd = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 100, 100, 0);
        mHandler.onInterceptTouchDownEvent(motionStart);

        assertOffsets(-100, -200);

        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_ENDED, 0.f, 0.f);
        mHandler.onPostDispatchDragEvent(DragEvent.ACTION_DRAG_ENDED);
        assertOffsets(0, 0);
    }

    @Test
    public void testHoverEvents() {
        MotionEvent hoverEnter =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 100, 100, 0);
        mHandler.onHoverEvent(hoverEnter);
        assertOffsets(-100, -200);

        MotionEvent hoverMove =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_MOVE, 100, 100, 0);
        mHandler.onHoverEvent(hoverMove);
        assertOffsets(-100, -200);

        MotionEvent hoverExit =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 100, 100, 0);
        mHandler.onHoverEvent(hoverExit);
        // Hover exit should NOT clear the offset, because a click might immediately follow.
        assertOffsets(-100, -200);

        MotionEvent actionUp = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 100, 100, 0);
        mHandler.onTouchEvent(actionUp);
        // Action up SHOULD clear the offset.
        assertOffsets(0, 0);
    }

    @Test
    public void testDragEventOffsets_withHorizontalOffset() {
        // Simulates layout with a horizontal offset (e.g. vertical tabs strip displayed).
        mViewport = new RectF(360, 150, 1000, 800);

        // Direct drag event without explicit offset.
        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_STARTED, 0.f, 0.f);
        assertOffsets(-360, -150);
        assertDragOffsets(0, 0);

        // Forwarded drag event with explicit horizontal offset (e.g. from context menu dialog).
        // Touch offset X should be 0 to prevent double offset, while Y preserves top control
        // offset.
        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_LOCATION, -360.f, 0.f);
        assertOffsets(0, -150);
        assertDragOffsets(-360, 0);

        // Drag ended clears both touch and drag offsets.
        mHandler.onPostDispatchDragEvent(DragEvent.ACTION_DRAG_ENDED);
        assertOffsets(0, 0);
        assertDragOffsets(0, 0);
    }

    @Test
    public void testDragEventOffsets_withoutHorizontalOffset() {
        // Simulates layout without a horizontal offset (e.g. vertical tabs hidden / phone layout).
        mViewport = new RectF(0, 150, 800, 800);

        // Direct drag event without explicit offset.
        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_STARTED, 0.f, 0.f);
        assertOffsets(0, -150);
        assertDragOffsets(0, 0);

        // Forwarded drag event with 0 offset.
        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_LOCATION, 0.f, 0.f);
        assertOffsets(0, -150);
        assertDragOffsets(0, 0);

        // Drag ended clears both touch and drag offsets.
        mHandler.onPostDispatchDragEvent(DragEvent.ACTION_DRAG_ENDED);
        assertOffsets(0, 0);
        assertDragOffsets(0, 0);
    }
}

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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the EventOffsetHandler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EventOffsetHandlerTest {
    private EventOffsetHandler mHandler;
    private EventOffsetHandler.EventOffsetHandlerDelegate mDelegate =
            new EventOffsetHandler.EventOffsetHandlerDelegate() {
                @Override
                public float getTop() {
                    return mViewport.top;
                }

                @Override
                public void setCurrentTouchEventOffsets(float top) {
                    mOffsetY = top;
                }

                @Override
                public void setCurrentDragEventOffsets(float dx, float dy) {}
            };

    private RectF mViewport;
    private float mOffsetY;

    private void assertOffsets(float y) {
        assertEquals(y, mOffsetY, 0.0);
    }

    @Before
    public void setUp() {
        mHandler = new EventOffsetHandler(mDelegate);
        mViewport = new RectF(100, 200, 600, 800);
        assertOffsets(0);
    }

    @Test
    public void testOffsetChangesWhileDragging() {
        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_STARTED, 0.f, 0.f);
        mHandler.onPostDispatchDragEvent(DragEvent.ACTION_DRAG_STARTED);

        // Viewport position has been negated.
        assertOffsets(-200);

        MotionEvent motionStart = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 100, 100, 0);
        mHandler.onInterceptTouchDownEvent(motionStart);

        assertOffsets(-200);

        MotionEvent motionEnd = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, 100, 100, 0);
        mHandler.onInterceptTouchDownEvent(motionStart);

        assertOffsets(-200);

        mHandler.onPreDispatchDragEvent(DragEvent.ACTION_DRAG_ENDED, 0.f, 0.f);
        mHandler.onPostDispatchDragEvent(DragEvent.ACTION_DRAG_ENDED);
        assertOffsets(0);
    }
}

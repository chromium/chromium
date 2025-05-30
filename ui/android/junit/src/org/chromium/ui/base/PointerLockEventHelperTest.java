// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.MotionEvent;
import android.view.Surface;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class PointerLockEventHelperTest {

    private PointerLockEventHelper mPointerLockEventHelper;

    @Before
    public void setUp() {
        mPointerLockEventHelper = new PointerLockEventHelper();
    }

    @Test
    public void testCapturedPointerTrackpadScrollEvent() {
        MotionEvent event = MotionEventTestUtils.getTrackpadEvent(MotionEvent.ACTION_MOVE, 0, 2);
        MotionEvent updatedEvent =
                PointerLockEventHelper.updateTrackpadCapturedScrollEvent(event, 10, -10);

        assertEquals(MotionEvent.ACTION_SCROLL, updatedEvent.getAction());
        assertTrue(updatedEvent.getAxisValue(MotionEvent.AXIS_HSCROLL) > 0);
        assertTrue(updatedEvent.getAxisValue(MotionEvent.AXIS_VSCROLL) < 0);
    }

    @Test
    public void testCapturedTrackpadMoveEvent() {
        float startX = 4;
        float startY = 10;
        float offsetX = 2;
        float offsetY = 5;

        MotionEvent event1 = MotionEventTestUtils.getCapturedTrackpadMoveEvent(startX, startY);
        MotionEvent event2 =
                MotionEventTestUtils.getCapturedTrackpadMoveEvent(
                        startX + offsetX, startY + offsetY);

        MotionEvent updatedEvent1 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event1, Surface.ROTATION_0);

        // First trackpad event should have a x&y = 0
        assertEquals(0, updatedEvent1.getX(), 0.01);
        assertEquals(0, updatedEvent1.getY(), 0.01);

        MotionEvent updatedEvent2 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event2, Surface.ROTATION_0);

        assertEquals(offsetX, updatedEvent2.getX(), 0.01);
        assertEquals(offsetY, updatedEvent2.getY(), 0.01);
    }

    @Test
    public void testCapturedTrackpadMoveEventWith90DegRotation() {
        float startX = 4;
        float startY = 10;
        float offsetX = 2;
        float offsetY = 5;

        MotionEvent event1 = MotionEventTestUtils.getCapturedTrackpadMoveEvent(startX, startY);
        MotionEvent event2 =
                MotionEventTestUtils.getCapturedTrackpadMoveEvent(
                        startX + offsetX, startY + offsetY);

        MotionEvent updatedEvent1 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event1, Surface.ROTATION_90);

        // First trackpad event should have a x&y = 0
        assertEquals(0, updatedEvent1.getX(), 0.01);
        assertEquals(0, updatedEvent1.getY(), 0.01);

        MotionEvent updatedEvent2 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event2, Surface.ROTATION_90);

        assertEquals(offsetY, updatedEvent2.getX(), 0.01);
        assertEquals(-offsetX, updatedEvent2.getY(), 0.01);
    }

    @Test
    public void testCapturedTrackpadMoveEventWith180DegRotation() {
        float startX = 4;
        float startY = 10;
        float offsetX = 2;
        float offsetY = 5;

        MotionEvent event1 = MotionEventTestUtils.getCapturedTrackpadMoveEvent(startX, startY);
        MotionEvent event2 =
                MotionEventTestUtils.getCapturedTrackpadMoveEvent(
                        startX + offsetX, startY + offsetY);

        MotionEvent updatedEvent1 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event1, Surface.ROTATION_180);

        // First trackpad event should have a x&y = 0
        assertEquals(0, updatedEvent1.getX(), 0.01);
        assertEquals(0, updatedEvent1.getY(), 0.01);

        MotionEvent updatedEvent2 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event2, Surface.ROTATION_180);

        assertEquals(-offsetX, updatedEvent2.getX(), 0.01);
        assertEquals(-offsetY, updatedEvent2.getY(), 0.01);
    }

    @Test
    public void testCapturedTrackpadMoveEventWith270DegRotation() {
        float startX = 4;
        float startY = 10;
        float offsetX = 2;
        float offsetY = 5;

        MotionEvent event1 = MotionEventTestUtils.getCapturedTrackpadMoveEvent(startX, startY);
        MotionEvent event2 =
                MotionEventTestUtils.getCapturedTrackpadMoveEvent(
                        startX + offsetX, startY + offsetY);

        MotionEvent updatedEvent1 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event1, Surface.ROTATION_270);

        // First trackpad event should have a x&y = 0
        assertEquals(0, updatedEvent1.getX(), 0.01);
        assertEquals(0, updatedEvent1.getY(), 0.01);

        MotionEvent updatedEvent2 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event2, Surface.ROTATION_270);

        assertEquals(-offsetY, updatedEvent2.getX(), 0.01);
        assertEquals(offsetX, updatedEvent2.getY(), 0.01);
    }
}

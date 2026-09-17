// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.Surface;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;

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

    @Test
    public void testCapturedPointerEventUpdatesRawCoordinates() {
        mPointerLockEventHelper.onNonCapturedPointerEvent(100f, 200f, 150f, 250f);
        assertEquals(100f, mPointerLockEventHelper.getLastPointerPositionX(), 0.01);
        assertEquals(200f, mPointerLockEventHelper.getLastPointerPositionY(), 0.01);
        assertEquals(150f, mPointerLockEventHelper.getLastPointerRawPositionXForTesting(), 0.01);
        assertEquals(250f, mPointerLockEventHelper.getLastPointerRawPositionYForTesting(), 0.01);

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
        assertEquals(100f, updatedEvent1.getX(), 0.01);
        assertEquals(200f, updatedEvent1.getY(), 0.01);
        assertEquals(150f, updatedEvent1.getRawX(), 0.01);
        assertEquals(250f, updatedEvent1.getRawY(), 0.01);

        MotionEvent updatedEvent2 =
                mPointerLockEventHelper.transformCapturedPointerEvent(event2, Surface.ROTATION_0);
        assertEquals(100f + offsetX, updatedEvent2.getX(), 0.01);
        assertEquals(200f + offsetY, updatedEvent2.getY(), 0.01);
        assertEquals(150f + offsetX, updatedEvent2.getRawX(), 0.01);
        assertEquals(250f + offsetY, updatedEvent2.getRawY(), 0.01);
    }

    @Test
    @EnableFeatures(UiAndroidFeatures.POINTER_LOCK_MOUSE_SCALING)
    public void testCapturedRelativeMouseEventAccumulatesHistoricalDeltas() {
        mPointerLockEventHelper.onNonCapturedPointerEvent(100f, 200f, 150f, 250f);

        MotionEvent event = createBatchedRelativeMouseEvent();

        assertEquals(2, event.getHistorySize());
        MotionEvent updatedEvent =
                mPointerLockEventHelper.transformCapturedPointerEvent(event, Surface.ROTATION_0);

        // X: (1 + 3 - 2) * 2.4 = 4.8; Y: (-2 + 4 + 5) * 2.4 = 16.8.
        assertEquals(104.8f, updatedEvent.getX(), 0.01);
        assertEquals(216.8f, updatedEvent.getY(), 0.01);
        assertEquals(154.8f, updatedEvent.getRawX(), 0.01);
        assertEquals(266.8f, updatedEvent.getRawY(), 0.01);
        assertEquals(InputDevice.SOURCE_MOUSE, updatedEvent.getSource());
    }

    @Test
    @DisableFeatures(UiAndroidFeatures.POINTER_LOCK_MOUSE_SCALING)
    public void testCapturedRelativeMouseEventDoesNotAccumulateWhenScalingDisabled() {
        mPointerLockEventHelper.onNonCapturedPointerEvent(100f, 200f, 150f, 250f);

        MotionEvent event = createBatchedRelativeMouseEvent();
        MotionEvent updatedEvent =
                mPointerLockEventHelper.transformCapturedPointerEvent(event, Surface.ROTATION_0);

        // Preserve the previous behavior by using only the current sample, (-2, 5), without
        // scaling.
        assertEquals(98f, updatedEvent.getX(), 0.01);
        assertEquals(205f, updatedEvent.getY(), 0.01);
        assertEquals(148f, updatedEvent.getRawX(), 0.01);
        assertEquals(255f, updatedEvent.getRawY(), 0.01);
        assertEquals(InputDevice.SOURCE_MOUSE, updatedEvent.getSource());
    }

    private static MotionEvent createBatchedRelativeMouseEvent() {
        MotionEvent.PointerProperties properties = new MotionEvent.PointerProperties();
        properties.id = 0;
        properties.toolType = MotionEvent.TOOL_TYPE_MOUSE;
        MotionEvent.PointerProperties[] propertiesList = {properties};

        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = 1f;
        coords.y = -2f;
        MotionEvent.PointerCoords[] coordsList = {coords};
        MotionEvent event =
                MotionEvent.obtain(
                        /* downTime= */ 100,
                        /* eventTime= */ 200,
                        MotionEvent.ACTION_MOVE,
                        /* pointerCount= */ 1,
                        propertiesList,
                        coordsList,
                        /* metaState= */ 0,
                        /* buttonState= */ 0,
                        /* xPrecision= */ 1f,
                        /* yPrecision= */ 1f,
                        /* deviceId= */ 1,
                        /* edgeFlags= */ 0,
                        InputDevice.SOURCE_MOUSE_RELATIVE,
                        /* flags= */ 0);

        coords.x = 3f;
        coords.y = 4f;
        event.addBatch(/* eventTime= */ 201, coordsList, /* metaState= */ 0);
        coords.x = -2f;
        coords.y = 5f;
        event.addBatch(/* eventTime= */ 202, coordsList, /* metaState= */ 0);
        return event;
    }
}

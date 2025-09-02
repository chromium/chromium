// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.InputDevice;
import android.view.MotionEvent;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link MotionEventUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MotionEventUtilsTest {

    @Test
    public void testIsPointerEvent() {
        assertTrue(
                "Mouse should be a pointer event.",
                MotionEventUtils.isPointerEvent(
                        createEventWithSourceAndToolType(
                                InputDevice.SOURCE_CLASS_POINTER, MotionEvent.TOOL_TYPE_MOUSE)));
        assertTrue(
                "Trackpad should be a pointer event.",
                MotionEventUtils.isPointerEvent(
                        createEventWithSourceAndToolType(
                                InputDevice.SOURCE_MOUSE, MotionEvent.TOOL_TYPE_FINGER)));

        assertFalse(
                "Touch without mouse source should not be a pointer event.",
                MotionEventUtils.isPointerEvent(
                        createEventWithSourceAndToolType(
                                InputDevice.SOURCE_CLASS_POINTER, MotionEvent.TOOL_TYPE_FINGER)));
        assertFalse(
                "Stylus should not be a pointer event.",
                MotionEventUtils.isPointerEvent(
                        createEventWithSourceAndToolType(
                                InputDevice.SOURCE_CLASS_POINTER, MotionEvent.TOOL_TYPE_STYLUS)));
        assertFalse(
                "Eraser should not be a pointer event.",
                MotionEventUtils.isPointerEvent(
                        createEventWithSourceAndToolType(
                                InputDevice.SOURCE_CLASS_POINTER, MotionEvent.TOOL_TYPE_ERASER)));
        assertFalse(
                "Unknown tool type should not be a pointer event.",
                MotionEventUtils.isPointerEvent(
                        createEventWithSourceAndToolType(
                                InputDevice.SOURCE_CLASS_POINTER, MotionEvent.TOOL_TYPE_UNKNOWN)));
    }

    private MotionEvent createEventWithSourceAndToolType(int source, int toolType) {
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = toolType;

        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = 100;
        coords[0].y = 200;

        return MotionEvent.obtain(
                0,
                0,
                MotionEvent.ACTION_DOWN,
                1,
                properties,
                coords,
                0,
                0,
                1f,
                1f,
                0,
                0,
                source,
                0);
    }
}

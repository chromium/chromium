// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;

import org.jni_zero.CalledByNative;
import org.junit.Assert;

import org.chromium.base.MathUtils;

public final class MotionEventTestUtils {
    public static MotionEvent getTrackpadTouchDownEventNoClick() {
        return getTrackpadEvent(MotionEvent.ACTION_DOWN, 0);
    }

    public static MotionEvent getTrackpadLeftClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_PRIMARY);
    }

    public static MotionEvent getTrackRightClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_SECONDARY);
    }

    public static MotionEvent getTrackpadEvent(int action, int buttonState) {
        return getTrackpadEvent(action, buttonState, 1);
    }

    public static MotionEvent getTrackpadEvent(int action, int buttonState, int pointersCnt) {
        return MotionEvent.obtain(
                0,
                0,
                action,
                pointersCnt,
                getToolTypeFingerProperties(pointersCnt),
                getPointerCoords(pointersCnt),
                0,
                buttonState,
                0,
                0,
                0,
                0,
                getTrackpadSource(),
                0);
    }

    public static MotionEvent getCapturedTrackpadMoveEvent(float x, float y) {
        MotionEvent event = MotionEventTestUtils.getTrackpadEvent(MotionEvent.ACTION_MOVE, 0);
        event.setLocation(x, y);
        event.setSource(InputDevice.SOURCE_TOUCHPAD);
        return event;
    }

    public static MotionEvent.PointerProperties[] getToolTypeFingerProperties(int pointersCnt) {
        MotionEvent.PointerProperties[] pointerPropertiesArray =
                new MotionEvent.PointerProperties[pointersCnt];
        for (int i = 0; i < pointersCnt; i++) {
            MotionEvent.PointerProperties trackpadProperties = new MotionEvent.PointerProperties();
            trackpadProperties.id = 7 + i;
            trackpadProperties.toolType = MotionEvent.TOOL_TYPE_FINGER;
            pointerPropertiesArray[i] = trackpadProperties;
        }
        return pointerPropertiesArray;
    }

    public static MotionEvent.PointerCoords[] getPointerCoords(int pointersCnt) {
        MotionEvent.PointerCoords[] pointerCoordsArray = new MotionEvent.PointerCoords[pointersCnt];
        for (int i = 0; i < pointersCnt; i++) {
            MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            coords.x = 14 + i;
            coords.y = 21 + i;
            pointerCoordsArray[i] = coords;
        }
        return pointerCoordsArray;
    }

    private static int getTrackpadSource() {
        return InputDevice.SOURCE_MOUSE;
    }

    public static void assertEquals(MotionEvent a, MotionEvent b) {
        if (a == b) {
            return;
        }
        Assert.assertNotNull(a);
        Assert.assertNotNull(b);

        Assert.assertEquals(a.getDownTime(), b.getDownTime());
        Assert.assertEquals(a.getEventTime(), b.getEventTime());
        Assert.assertEquals(a.getAction(), b.getAction());
        Assert.assertEquals(a.getPointerCount(), b.getPointerCount());
        Assert.assertEquals(a.getMetaState(), b.getMetaState());
        Assert.assertEquals(a.getButtonState(), b.getButtonState());
        Assert.assertEquals(a.getXPrecision(), b.getXPrecision(), MathUtils.EPSILON);
        Assert.assertEquals(a.getYPrecision(), b.getYPrecision(), MathUtils.EPSILON);
        Assert.assertEquals(a.getDeviceId(), b.getDeviceId());
        Assert.assertEquals(a.getEdgeFlags(), b.getEdgeFlags());
        Assert.assertEquals(a.getSource(), b.getSource());
        Assert.assertEquals(a.getFlags(), b.getFlags());
        Assert.assertEquals(a.getClassification(), b.getClassification());

        for (int i = 0; i < a.getPointerCount(); i++) {
            MotionEvent.PointerProperties aProps = new MotionEvent.PointerProperties();
            MotionEvent.PointerProperties bProps = new MotionEvent.PointerProperties();
            a.getPointerProperties(i, aProps);
            b.getPointerProperties(i, bProps);
            MotionEvent.PointerCoords aCoords = new MotionEvent.PointerCoords();
            MotionEvent.PointerCoords bCoords = new MotionEvent.PointerCoords();
            a.getPointerCoords(i, aCoords);
            b.getPointerCoords(i, bCoords);

            Assert.assertEquals(aProps, bProps);
            assertEquals(aCoords, bCoords);
        }
    }

    private static void assertEquals(MotionEvent.PointerCoords a, MotionEvent.PointerCoords b) {
        if (a == b) {
            return;
        }

        Assert.assertNotNull(a);
        Assert.assertNotNull(b);

        Assert.assertEquals(a.orientation, b.orientation, MathUtils.EPSILON);
        Assert.assertEquals(a.pressure, b.pressure, MathUtils.EPSILON);
        Assert.assertEquals(a.size, b.size, MathUtils.EPSILON);
        Assert.assertEquals(a.toolMajor, b.toolMajor, MathUtils.EPSILON);
        Assert.assertEquals(a.toolMinor, b.toolMinor, MathUtils.EPSILON);
        Assert.assertEquals(a.touchMajor, b.touchMajor, MathUtils.EPSILON);
        Assert.assertEquals(a.touchMinor, b.touchMinor, MathUtils.EPSILON);
        Assert.assertEquals(a.x, b.x, MathUtils.EPSILON);
        Assert.assertEquals(a.y, b.y, MathUtils.EPSILON);
    }

    @CalledByNative
    private static MotionEvent getMultiTouchEventWithHistory(
            long oldestEventTime, long latestEventTime, long downTime) {
        // Any value greater than `MotionEventAndroid::kDefaultCachedPointers`.
        int pointerCount = 3;
        PointerProperties[] pointerProperties =
                new PointerProperties[] {
                    new PointerProperties(), new PointerProperties(), new PointerProperties()
                };
        PointerCoords[] pointerCoords =
                new PointerCoords[] {new PointerCoords(), new PointerCoords(), new PointerCoords()};
        assert (pointerProperties.length == pointerCoords.length);
        assert (pointerCoords.length == pointerCount);
        // Set non-zero values for `pointerCoords`.
        for (int i = 0; i < pointerCoords.length; i++) {
            pointerCoords[i].x += 5 + i;
            pointerCoords[i].y += 5 + i;
            pointerCoords[i].touchMajor += 5 + i;
        }
        MotionEvent event =
                MotionEvent.obtain(
                        downTime,
                        oldestEventTime,
                        MotionEvent.ACTION_MOVE,
                        pointerCoords.length,
                        pointerProperties,
                        pointerCoords,
                        0,
                        0,
                        1.0f,
                        1.0f,
                        0,
                        0,
                        InputDevice.SOURCE_CLASS_POINTER,
                        0);

        // Set x,y values different than the ones already present in MotionEvent.
        // The values already present in MotionEvent could be accessed with `getHistoricalXXX`
        // methods.
        for (int i = 0; i < event.getPointerCount(); i++) {
            pointerCoords[i].x += 5 + i;
            pointerCoords[i].y += 5 + i;
            pointerCoords[i].touchMajor += 5 + i;
        }
        event.addBatch(latestEventTime, pointerCoords, 0);
        event.offsetLocation(/* deltaX= */ 10, /* deltaY= */ 20);
        return event;
    }
}

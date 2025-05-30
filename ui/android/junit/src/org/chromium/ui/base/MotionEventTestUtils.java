// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.InputDevice;
import android.view.MotionEvent;

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
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.Surface;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;

@NullMarked
public final class PointerLockEventHelper {

    // Holds the previous pointer event's position that was forwarded to native
    private float mLastPointerPositionX;
    private float mLastPointerPositionY;

    // Holds the previous trackpad event's position when the pointer is captured, the event's
    // position in this case contains the raw finger coordinates on the trackpad
    private float mLastTrackpadPositionX;
    private float mLastTrackpadPositionY;

    private boolean mIsLastTrackpadPositionValid;

    // Called whenever we have a new mouse event when the pointer is not locked. Needed for updating
    // the state of the pointer & trackpad variables that are used in calculating the correct
    // pointer position when the pointer is captured
    public void onNonCapturedPointerEvent(float x, float y) {
        updateLastPointerPosition(x, y);
        mIsLastTrackpadPositionValid = false;
    }

    // Updates the last pointer position that was forwarded to the native side
    public void updateLastPointerPosition(float x, float y) {
        mLastPointerPositionX = x;
        mLastPointerPositionY = y;
    }

    public float getLastPointerPositionX() {
        return mLastPointerPositionX;
    }

    public float getLastPointerPositionY() {
        return mLastPointerPositionY;
    }

    public MotionEvent transformCapturedPointerEvent(MotionEvent event, int deviceRotation) {
        float offsetX = 0.0f;
        float offsetY = 0.0f;

        if (event.isFromSource(InputDevice.SOURCE_TOUCHPAD)) {
            // Ignore calculating the offset if we don't have the previous event trackpad position
            if (mIsLastTrackpadPositionValid) {
                // Input device is trackpad, getX & getY return the raw finger position on the
                // trackpad
                // Calculate the offsets based on the previous event position vs. the current event
                // position
                offsetX = event.getX() - mLastTrackpadPositionX;
                offsetY = event.getY() - mLastTrackpadPositionY;
            }

            mLastTrackpadPositionX = event.getX();
            mLastTrackpadPositionY = event.getY();

            float tempOffsetX = offsetX;

            offsetX = getOffsetXBasedOnDeviceRotation(offsetX, offsetY, deviceRotation);
            offsetY = getOffsetYBasedOnDeviceRotation(tempOffsetX, offsetY, deviceRotation);

            event = updateTrackpadEvent(event, offsetX, offsetY);

            // Cancel any calculated offset, since scroll events shouldn't affect trackpad position
            if (event.getAction() == MotionEvent.ACTION_SCROLL) {
                offsetX = 0;
                offsetY = 0;
            }

            // Invalidate the trackpad position for these cases:
            // ACTION_UP: No pointer on the trackpad, the position data is stale
            // ACTION_POINTER_UP & ACTION_POINTER_DOWN: Multiple trackpad pointers causes the main
            // pointer (pointer at idx 0) to flip from one to another, causing sudden jumps in
            // offsets, we need to wait for a subsequent event to figure out the correct trackpad
            // position
            mIsLastTrackpadPositionValid =
                    (event.getAction() != MotionEvent.ACTION_UP
                            && event.getActionMasked() != MotionEvent.ACTION_POINTER_UP
                            && event.getActionMasked() != MotionEvent.ACTION_POINTER_DOWN);
        } else if (event.isFromSource(InputDevice.SOURCE_MOUSE_RELATIVE)) {
            // Input device is Mouse, getX & getY return the relative change of the pointer position
            offsetX = event.getX();
            offsetY = event.getY();
        } else {
            // Unexpected source
            return event;
        }

        float currentPointerPositionX = mLastPointerPositionX + offsetX;
        float currentPointerPositionY = mLastPointerPositionY + offsetY;

        MotionEvent ret = MotionEvent.obtain(event);
        ret.setSource(InputDevice.SOURCE_MOUSE);
        ret.setLocation(currentPointerPositionX, currentPointerPositionY);

        return ret;
    }

    private static MotionEvent updateTrackpadEvent(
            MotionEvent event, float offsetX, float offsetY) {
        MotionEvent updatedEvent = updateTrackpadCapturedButtonState(event);
        return updateTrackpadCapturedScrollEvent(updatedEvent, offsetX, offsetY);
    }

    private static MotionEvent updateTrackpadCapturedButtonState(MotionEvent event) {
        if (event.getAction() != MotionEvent.ACTION_BUTTON_PRESS
                && event.getAction() != MotionEvent.ACTION_BUTTON_RELEASE) {
            return event;
        }

        // When the pointer is captured, all clicks on trackpad would have a BUTTON_PRIMARY state,
        // regardless of how many pointers are on the trackpad. So, we need to correct the button
        // state
        int updatedButtonState;
        if (event.getPointerCount() == 2) {
            updatedButtonState = MotionEvent.BUTTON_SECONDARY;
        } else if (event.getPointerCount() == 3) {
            updatedButtonState = MotionEvent.BUTTON_TERTIARY;
        } else {
            // No change is made on the event
            return event;
        }

        return MotionEvent.obtain(
                event.getDownTime(),
                event.getEventTime(),
                event.getAction(),
                event.getPointerCount(),
                getPointerPropertiesForEvent(event),
                getPointerCoordsForEvent(event),
                event.getMetaState(),
                updatedButtonState,
                event.getXPrecision(),
                event.getYPrecision(),
                event.getDeviceId(),
                event.getEdgeFlags(),
                event.getSource(),
                event.getFlags());
    }

    // TODO(https://crbug.com/415730929): Scroll movement has no momentum
    // When the pointer is captured, multi-touch gestures are not supported, this supports 2 finger
    // move to count as a scroll gesture
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static MotionEvent updateTrackpadCapturedScrollEvent(
            MotionEvent event, float offsetX, float offsetY) {
        if (event.getAction() != MotionEvent.ACTION_MOVE
                || event.getPointerCount() != 2
                || (offsetX == 0 && offsetY == 0)) {
            return event;
        }

        // Resolution is how many pixels per millimeters on the trackpad
        float xAxisResolution = 1;
        float yAxisResolution = 1;

        InputDevice.MotionRange xAxisMotionRange =
                DeviceInput.getTouchpadXAxisMotionRange(event.getDeviceId());
        InputDevice.MotionRange yAxisMotionRange =
                DeviceInput.getTouchpadYAxisMotionRange(event.getDeviceId());

        if (xAxisMotionRange != null) {
            xAxisResolution = xAxisMotionRange.getResolution();
        }
        if (yAxisMotionRange != null) {
            yAxisResolution = yAxisMotionRange.getResolution();
        }

        // TODO(https://crbug.com/415730929): inverse scrolling is not respected, doesn't seem that
        // the setting value is exposed from the OS.
        float xDirection = MathUtils.clamp(offsetX / xAxisResolution, -1, 1);
        float yDirection = MathUtils.clamp(offsetY / yAxisResolution, -1, 1);

        MotionEvent.PointerCoords updatedPointerCoords = new MotionEvent.PointerCoords();
        event.getPointerCoords(0, updatedPointerCoords);
        updatedPointerCoords.setAxisValue(MotionEvent.AXIS_HSCROLL, xDirection);
        updatedPointerCoords.setAxisValue(MotionEvent.AXIS_VSCROLL, yDirection);

        MotionEvent.PointerCoords[] updatedPointerCoordsList = getPointerCoordsForEvent(event);
        updatedPointerCoordsList[0] = updatedPointerCoords;

        return MotionEvent.obtain(
                event.getDownTime(),
                event.getEventTime(),
                MotionEvent.ACTION_SCROLL,
                event.getPointerCount(),
                getPointerPropertiesForEvent(event),
                updatedPointerCoordsList,
                event.getMetaState(),
                event.getButtonState(),
                event.getXPrecision(),
                event.getYPrecision(),
                event.getDeviceId(),
                event.getEdgeFlags(),
                event.getSource(),
                event.getFlags());
    }

    private static MotionEvent.PointerProperties[] getPointerPropertiesForEvent(MotionEvent event) {
        MotionEvent.PointerProperties[] ret =
                new MotionEvent.PointerProperties[event.getPointerCount()];
        for (int i = 0; i < event.getPointerCount(); i++) {
            MotionEvent.PointerProperties properties = new MotionEvent.PointerProperties();
            event.getPointerProperties(i, properties);

            ret[i] = properties;
        }
        return ret;
    }

    private static MotionEvent.PointerCoords[] getPointerCoordsForEvent(MotionEvent event) {
        MotionEvent.PointerCoords[] ret = new MotionEvent.PointerCoords[event.getPointerCount()];
        for (int i = 0; i < event.getPointerCount(); i++) {
            MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            event.getPointerCoords(i, coords);

            ret[i] = coords;
        }
        return ret;
    }

    private static float getOffsetXBasedOnDeviceRotation(
            float offsetX, float offsetY, int rotation) {
        return switch (rotation) {
            case Surface.ROTATION_0 -> offsetX;
            case Surface.ROTATION_90 -> offsetY;
            case Surface.ROTATION_180 -> -offsetX;
            case Surface.ROTATION_270 -> -offsetY;
            default -> offsetX; // unreachable
        };
    }

    private static float getOffsetYBasedOnDeviceRotation(
            float offsetX, float offsetY, int rotation) {
        return switch (rotation) {
            case Surface.ROTATION_0 -> offsetY;
            case Surface.ROTATION_90 -> -offsetX;
            case Surface.ROTATION_180 -> -offsetY;
            case Surface.ROTATION_270 -> offsetX;
            default -> offsetY; // unreachable
        };
    }
}

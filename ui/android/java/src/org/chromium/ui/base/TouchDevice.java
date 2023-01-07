// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.pm.PackageManager;
import android.view.InputDevice;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Simple proxy for querying input device properties from C++.
 */
@JNINamespace("ui")
public class TouchDevice {

    /**
     * Static methods only so make constructor private.
     */
    private TouchDevice() { }

    /**
     * @return Maximum supported touch points.
     */
    @CalledByNative
    private static int maxTouchPoints() {
        // Android only tells us if the device belongs to a "Touchscreen Class" which only
        // guarantees a minimum number of touch points. Be conservative and return the minimum,
        // checking membership from the highest class down.

        if (ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
                    PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH_JAZZHAND)) {
            return 5;
        } else if (ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
                           PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH_DISTINCT)) {
            return 2;
        } else if (ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
                           PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH)) {
            return 2;
        } else if (ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
                           PackageManager.FEATURE_TOUCHSCREEN)) {
            return 1;
        } else {
            return 0;
        }
    }

    /**
     * @return an array of two ints: result[0] represents the pointer-types and result[1] represents
     *         the hover-types supported by the device, where each int is the union (bitwise OR) of
     *         corresponding type (PointerType/HoverType) bits.
     */
    @CalledByNative
    private static int[] availablePointerAndHoverTypes() {
        int[] result = new int[2];
        result[0] = result[1] = 0;

        for (int deviceId : InputDevice.getDeviceIds()) {
            InputDevice inputDevice = null;
            try {
                inputDevice = InputDevice.getDevice(deviceId);
            } catch (RuntimeException e) {
                // Swallow the exception. See crbug.com/781377.
            }
            if (inputDevice == null) continue;

            int sources = inputDevice.getSources();

            if (hasSource(sources, InputDevice.SOURCE_MOUSE)
                    || hasSource(sources, InputDevice.SOURCE_STYLUS)
                    || hasSource(sources, InputDevice.SOURCE_TOUCHPAD)
                    || hasSource(sources, InputDevice.SOURCE_TRACKBALL)) {
                result[0] |= PointerType.FINE;
            } else if (hasSource(sources, InputDevice.SOURCE_TOUCHSCREEN)) {
                result[0] |= PointerType.COARSE;
            }

            if (hasSource(sources, InputDevice.SOURCE_MOUSE)
                    || hasSource(sources, InputDevice.SOURCE_TOUCHPAD)
                    || hasSource(sources, InputDevice.SOURCE_TRACKBALL)) {
                result[1] |= HoverType.HOVER;
            }

            // Remaining InputDevice sources: SOURCE_DPAD, SOURCE_GAMEPAD, SOURCE_JOYSTICK,
            // SOURCE_KEYBOARD, SOURCE_TOUCH_NAVIGATION, SOURCE_UNKNOWN
        }

        if (result[0] == 0) result[0] = PointerType.NONE;
        if (result[1] == 0) result[1] = HoverType.NONE;

        return result;
    }

    private static boolean hasSource(int sources, int inputDeviceSource) {
        return (sources & inputDeviceSource) == inputDeviceSource;
    }
}

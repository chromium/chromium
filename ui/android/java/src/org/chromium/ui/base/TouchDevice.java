// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.pm.PackageManager;
import android.view.InputDevice;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/** Simple proxy for querying input device properties from C++. */
@JNINamespace("ui")
public class TouchDevice {

    /** Static methods only so make constructor private. */
    private TouchDevice() {}

    /**
     * @return Maximum supported touch points.
     */
    @CalledByNative
    private static int maxTouchPoints() {
        // Android only tells us if the device belongs to a "Touchscreen Class" which only
        // guarantees a minimum number of touch points. Be conservative and return the minimum,
        // checking membership from the highest class down.

        if (ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH_JAZZHAND)) {
            return 5;
        } else if (ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH_DISTINCT)) {
            return 2;
        } else if (ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH)) {
            return 2;
        } else if (ContextUtils.getApplicationContext()
                .getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)) {
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
        int pointerTypes = 0;
        int hoverTypes = 0;

        for (int deviceId : InputDevice.getDeviceIds()) {
            InputDevice inputDevice = null;
            try {
                inputDevice = InputDevice.getDevice(deviceId);
            } catch (RuntimeException e) {
                // Swallow the exception. See crbug.com/781377.
            }
            if (inputDevice == null) continue;

            int sources = inputDevice.getSources();
            boolean isFinePointer =
                    hasSource(sources, InputDevice.SOURCE_MOUSE)
                            || hasSource(sources, InputDevice.SOURCE_STYLUS)
                            || hasSource(sources, InputDevice.SOURCE_TOUCHPAD)
                            || hasSource(sources, InputDevice.SOURCE_TRACKBALL);
            if (isFinePointer) {
                pointerTypes |= PointerType.FINE;
            }
            if (hasSource(sources, InputDevice.SOURCE_TOUCHSCREEN)
                    && (UiAndroidFeatureMap.isEnabled(
                                    UiAndroidFeatures.REPORT_ALL_AVAILABLE_POINTER_TYPES)
                            || !isFinePointer)) {
                pointerTypes |= PointerType.COARSE;
            }

            if (hasSource(sources, InputDevice.SOURCE_MOUSE)
                    || hasSource(sources, InputDevice.SOURCE_TOUCHPAD)
                    || hasSource(sources, InputDevice.SOURCE_TRACKBALL)) {
                hoverTypes |= HoverType.HOVER;
            }

            // Remaining InputDevice sources: SOURCE_DPAD, SOURCE_GAMEPAD, SOURCE_JOYSTICK,
            // SOURCE_KEYBOARD, SOURCE_TOUCH_NAVIGATION, SOURCE_UNKNOWN
        }

        if (pointerTypes == 0) pointerTypes = PointerType.NONE;
        if (hoverTypes == 0) hoverTypes = HoverType.NONE;

        return new int[] {pointerTypes, hoverTypes};
    }

    private static boolean hasSource(int sources, int inputDeviceSource) {
        return (sources & inputDeviceSource) == inputDeviceSource;
    }
}

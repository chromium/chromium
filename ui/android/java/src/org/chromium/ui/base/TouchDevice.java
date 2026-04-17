// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.UiModeManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.view.InputDevice;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;

/** Simple proxy for querying input device properties from C++. */
@JNINamespace("ui")
@NullMarked
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
     *     the hover-types supported by the device, where each int is the union (bitwise OR) of
     *     corresponding type (PointerType/HoverType) bits. This is capability-oriented (for example
     *     SOURCE_STYLUS can be reported even when no stylus is currently active).
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

            if (inputDevice.isVirtual() || !inputDevice.isEnabled()) {
                continue;
            }

            int sources = inputDevice.getSources();
            // Include SOURCE_MOUSE and additional mouse-like sources used for
            // pointer/hover capability.
            boolean hasMouseLikeSource =
                    hasSource(sources, InputDevice.SOURCE_MOUSE)
                            || hasSource(sources, InputDevice.SOURCE_TOUCHPAD)
                            || hasSource(sources, InputDevice.SOURCE_TRACKBALL);
            // This method reports supported pointer capabilities, not only currently connected
            // accessories. On Android, SOURCE_STYLUS can indicate stylus capability even when a
            // stylus is not currently connected/active.
            boolean isFinePointer =
                    hasMouseLikeSource || hasSource(sources, InputDevice.SOURCE_STYLUS);
            if (isFinePointer) {
                pointerTypes |= PointerType.FINE;
            }
            if (hasSource(sources, InputDevice.SOURCE_TOUCHSCREEN)) {
                pointerTypes |= PointerType.COARSE;
            }

            if (hasMouseLikeSource && isRealPointerDevice(inputDevice, sources)) {
                hoverTypes |= HoverType.HOVER;
            }

            // Remaining InputDevice sources: SOURCE_DPAD, SOURCE_GAMEPAD, SOURCE_JOYSTICK,
            // SOURCE_KEYBOARD, SOURCE_TOUCH_NAVIGATION, SOURCE_UNKNOWN
        }

        if (pointerTypes == 0) pointerTypes = PointerType.NONE;
        if (hoverTypes == 0) hoverTypes = HoverType.NONE;

        return new int[] {pointerTypes, hoverTypes};
    }

    /**
     * Returns true if the device should contribute hover capability.
     *
     * <p>On desktop-like Android sessions, built-in SOURCE_MOUSE devices are real and should be
     * counted (for example Android Desktop and Samsung DeX).
     *
     * <p>On non-desktop Android, filters out bogus built-in OEM input devices that falsely report
     * SOURCE_MOUSE. See https://crbug.com/41445959. On API 29+, uses isExternal() to identify USB
     * and Bluetooth devices. On older APIs, falls back to rejecting devices that report both
     * SOURCE_MOUSE and SOURCE_TOUCHSCREEN.
     *
     * <p>Known gap: Samsung DeX mode where the phone acts as a trackpad (tested on S24 Ultra)
     * reports both isExternal()=false and UI_MODE_TYPE_DESK=false, so isRealPointerDevice() returns
     * false. See crbug.com/502461774.
     */
    private static boolean isRealPointerDevice(InputDevice device, int sources) {
        if (isDesktopLikeMode()) {
            return true;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return device.isExternal();
        }
        // Pre-Q fallback: a device claiming both mouse and touchscreen is bogus.
        return !hasSource(sources, InputDevice.SOURCE_TOUCHSCREEN);
    }

    private static boolean isDesktopLikeMode() {
        // Approved exception for ui/base: crbug.com/494225648.
        // UI_MODE_TYPE_DESK covers DeX-like desktop sessions where built-in
        // pointer devices may not be reported as external. Samsung's own docs
        // recommend this check
        // (https://developer.samsung.com/sdp/blog/en/2017/07/27/samsung-dex-how-to-detect-the-samsung-dex-mode),
        // though testing on S24 Ultra showed it returns false for DeX trackpad
        // mode. Kept in case other Samsung configurations do report it.
        // TODO(crbug.com/502461774): Find a reliable DeX detection heuristic
        // that covers the phone-as-trackpad case.
        if (DeviceInfo.isDesktop()) { // nocheck
            return true;
        }
        UiModeManager uiModeManager =
                (UiModeManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.UI_MODE_SERVICE);
        return uiModeManager != null
                && uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_DESK;
    }

    private static boolean hasSource(int sources, int inputDeviceSource) {
        return (sources & inputDeviceSource) == inputDeviceSource;
    }
}

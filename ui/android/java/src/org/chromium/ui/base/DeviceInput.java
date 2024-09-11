// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC;
import static android.view.InputDevice.SOURCE_MOUSE;

import android.content.Context;
import android.hardware.input.InputManager;
import android.hardware.input.InputManager.InputDeviceListener;
import android.util.SparseArray;
import android.view.InputDevice;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

/**
 * Utilities for accessing device input information. Note that this class is not thread-safe and
 * currently asserts all interactions occur on the UI thread. If usage is required off the UI thread
 * in the future, this class can be modified for multi-thread support.
 */
public class DeviceInput implements InputDeviceListener {

    /** Wrapper class which provides lazy initialization of a singleton instance. */
    private static final class LazyInit {
        public static final DeviceInput sInstance = new DeviceInput();
    }

    /** See {@link #setSupportsAlphabeticKeyboardForTesting(boolean)}. */
    private static Boolean sSupportsAlphabeticKeyboardForTesting;

    /** See {@link #setSupportsPrevisionPointerForTesting(boolean)}. */
    private static Boolean sSupportsPrecisionPointerForTesting;

    /** Cached snapshots of all currently connected {@link InputDevice}s. */
    private final SparseArray<DeviceSnapshot> mDeviceSnapshotsById = new SparseArray<>();

    /** Only a lazy singleton instance may be instantiated. */
    private DeviceInput() {
        ThreadUtils.assertOnUiThread();

        // Initialize cache.
        final int[] deviceIds = InputDevice.getDeviceIds();
        for (int i = 0; i < deviceIds.length; i++) {
            int deviceId = deviceIds[i];
            var snapshot = DeviceSnapshot.from(InputDevice.getDevice(deviceId));
            mDeviceSnapshotsById.put(deviceId, snapshot);
        }

        // Register listener to perform cache updates.
        var context = ContextUtils.getApplicationContext();
        var inputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, /* handler= */ null);
    }

    /** Returns a lazily instantiated singleton instance. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static DeviceInput getInstance() {
        ThreadUtils.assertOnUiThread();
        return LazyInit.sInstance;
    }

    /** Modifies the output of {@link #supportsAlphabeticKeyboard()} for testing. */
    public static void setSupportsAlphabeticKeyboardForTesting(Boolean supportsAlphabeticKeyboard) {
        sSupportsAlphabeticKeyboardForTesting = supportsAlphabeticKeyboard;
        ResettersForTesting.register(() -> sSupportsAlphabeticKeyboardForTesting = null);
    }

    /**
     * @return Whether any currently connected {@link InputDevice} supports an alphabetic keyboard.
     */
    public static boolean supportsAlphabeticKeyboard() {
        ThreadUtils.assertOnUiThread();
        return getInstance().supportsAlphabeticKeyboardImpl();
    }

    /** Implementation of {@link #supportsAlphabeticKeyboard()}. */
    public boolean supportsAlphabeticKeyboardImpl() {
        ThreadUtils.assertOnUiThread();
        if (sSupportsAlphabeticKeyboardForTesting != null) {
            return sSupportsAlphabeticKeyboardForTesting;
        }
        for (int i = 0; i < mDeviceSnapshotsById.size(); i++) {
            if (mDeviceSnapshotsById.valueAt(i).supportsAlphabeticKeyboard) return true;
        }
        return false;
    }

    /** Modifies the output of {@link #supportsPrecisionPointer()} for testing. */
    public static void setSupportsPrecisionPointerForTesting(Boolean supportsPrecisionPointer) {
        sSupportsPrecisionPointerForTesting = supportsPrecisionPointer;
        ResettersForTesting.register(() -> sSupportsPrecisionPointerForTesting = null);
    }

    /**
     * @return Whether any currently connected {@link InputDevice} supports precision pointing. Note
     *     that this includes not only mice, but also any mice-like pointing devices (e.g. stylus,
     *     touchpad, etc).
     */
    public static boolean supportsPrecisionPointer() {
        ThreadUtils.assertOnUiThread();
        return getInstance().supportsPrecisionPointerImpl();
    }

    /** Implementation of {@link #supportsPrecisionPointer()}. */
    private boolean supportsPrecisionPointerImpl() {
        ThreadUtils.assertOnUiThread();
        if (sSupportsPrecisionPointerForTesting != null) {
            return sSupportsPrecisionPointerForTesting;
        }
        for (int i = 0; i < mDeviceSnapshotsById.size(); i++) {
            if (mDeviceSnapshotsById.valueAt(i).supportsPrecisionPointer) return true;
        }
        return false;
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        ThreadUtils.assertOnUiThread();
        mDeviceSnapshotsById.put(deviceId, DeviceSnapshot.from(InputDevice.getDevice(deviceId)));
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        ThreadUtils.assertOnUiThread();
        mDeviceSnapshotsById.put(deviceId, DeviceSnapshot.from(InputDevice.getDevice(deviceId)));
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        ThreadUtils.assertOnUiThread();
        mDeviceSnapshotsById.remove(deviceId);
    }

    /** Class which represents a snapshot of given {@link InputDevice}. */
    private static final class DeviceSnapshot {

        /** Whether the associated {@link InputDevice} supports an alphabetic keyboard. */
        public final boolean supportsAlphabeticKeyboard;

        /**
         * Whether the associated {@link InputDevice} supports precision pointing. Note that this
         * includes not only mice, but also any mice-like pointing devices (e.g. stylus, touchpad,
         * etc).
         */
        public final boolean supportsPrecisionPointer;

        /** See {@link #from(InputDevice)}. */
        private DeviceSnapshot(
                boolean supportsAlphabeticKeyboard, boolean supportsPrecisionPointer) {
            this.supportsAlphabeticKeyboard = supportsAlphabeticKeyboard;
            this.supportsPrecisionPointer = supportsPrecisionPointer;
        }

        /**
         * @return a new snapshot representing the specified {@link InputDevice}.
         */
        public static DeviceSnapshot from(InputDevice device) {
            boolean isPhysical = !device.isVirtual();
            return new DeviceSnapshot(
                    /* supportsAlphabeticKeyboard= */ isPhysical
                            && device.getKeyboardType() == KEYBOARD_TYPE_ALPHABETIC,
                    /* supportsPrecisionPointer= */ isPhysical
                            && device.supportsSource(SOURCE_MOUSE));
        }
    }
}

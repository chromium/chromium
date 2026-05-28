// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC;
import static android.view.InputDevice.KEYBOARD_TYPE_NONE;
import static android.view.InputDevice.SOURCE_MOUSE;
import static android.view.InputDevice.SOURCE_TOUCHPAD;

import android.content.Context;
import android.content.res.Configuration;
import android.hardware.input.InputManager;
import android.hardware.input.InputManager.InputDeviceListener;
import android.util.SparseArray;
import android.view.InputDevice;
import android.view.MotionEvent;

import androidx.annotation.GuardedBy;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Utilities for accessing device input information.
 */
@NullMarked
public class DeviceInput implements InputDeviceListener {

    /** Wrapper class which provides lazy initialization of a singleton instance. */
    private static final class LazyInit {
        public static final DeviceInput sInstance = new DeviceInput();
    }

    /** See {@link #setSupportsAlphabeticKeyboardForTesting(boolean)}. */
    private static @Nullable Boolean sSupportsAlphabeticKeyboardForTesting;

    /** See {@link #setSupportsKeyboardForTesting(boolean)}. */
    private static @Nullable Boolean sSupportsKeyboardForTesting;

    /** See {@link #setSupportsPrevisionPointerForTesting(boolean)}. */
    private static @Nullable Boolean sSupportsPrecisionPointerForTesting;

    /** Cached snapshots of all currently connected {@link InputDevice}s. */
    @GuardedBy("mLock")
    private final SparseArray<DeviceSnapshot> mDeviceSnapshotsById = new SparseArray<>();

    private final Object mLock = new Object();

    /** Only a lazy singleton instance may be instantiated. */
    private DeviceInput() {
        // Initialize cache.
        final int[] deviceIds = InputDevice.getDeviceIds();
        for (int i = 0; i < deviceIds.length; i++) {
            int deviceId = deviceIds[i];
            InputDevice device = InputDevice.getDevice(deviceId);
            if (device != null) {
                mDeviceSnapshotsById.put(deviceId, DeviceSnapshot.from(device));
            }
        }

        // Register listener to perform cache updates.
        var context = ContextUtils.getApplicationContext();
        var inputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, ThreadUtils.getUiThreadHandler());
    }

    /** Returns a lazily instantiated singleton instance. */
    @VisibleForTesting
    public static DeviceInput getInstance() {
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
        return getInstance().supportsAlphabeticKeyboardImpl();
    }

    /** Modifies the output of {@link #supportsKeyboard()} for testing. */
    public static void setSupportsKeyboardForTesting(Boolean supportsKeyboard) {
        sSupportsKeyboardForTesting = supportsKeyboard;
        // Register a callback to reset this value after every test method completes.
        ResettersForTesting.register(() -> sSupportsKeyboardForTesting = null);
    }

    /**
     * Checks if a numeric or alphabetic keyboard is currently attached and usable.
     *
     * @return true if a physical keyboard (QWERTY or 12-key) is active and not hidden.
     */
    public static boolean supportsKeyboard(Context context) {
        return getInstance().supportsKeyboardImpl(context);
    }

    /** Implementation of {@link #supportsAlphabeticKeyboard()}. */
    public boolean supportsAlphabeticKeyboardImpl() {
        synchronized (mLock) {
            if (sSupportsAlphabeticKeyboardForTesting != null) {
                return sSupportsAlphabeticKeyboardForTesting;
            }
            for (int i = 0; i < mDeviceSnapshotsById.size(); i++) {
                if (mDeviceSnapshotsById.valueAt(i).supportsAlphabeticKeyboard) {
                    return true;
                }
            }
            return false;
        }
    }

    /** Implementation of {@link #supportsKeyboard()}. */
    public boolean supportsKeyboardImpl(Context context) {
        synchronized (mLock) {
            // TODO(crbug.com/479570578): Remove the flag after this change is stalbe for awhile
            if (UiAndroidFeatureList.sSupportKeyboard.isEnabled()) {
                if (sSupportsKeyboardForTesting != null) {
                    return sSupportsKeyboardForTesting;
                }

                Configuration config = context.getResources().getConfiguration();
                boolean hasKeyboard =
                        config.keyboard == Configuration.KEYBOARD_QWERTY
                                || config.keyboard == Configuration.KEYBOARD_12KEY;
                boolean isUncovered =
                        config.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_NO;
                return hasKeyboard && isUncovered;
            } else {
                if (sSupportsKeyboardForTesting != null) {
                    return sSupportsKeyboardForTesting;
                }

                for (int i = 0; i < mDeviceSnapshotsById.size(); i++) {
                    if (mDeviceSnapshotsById.valueAt(i).supportsKeyboard) {
                        return true;
                    }
                }
                return false;
            }
        }
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
        return getInstance().supportsPrecisionPointerImpl();
    }

    /** Implementation of {@link #supportsPrecisionPointer()}. */
    private boolean supportsPrecisionPointerImpl() {
        synchronized (mLock) {
            if (sSupportsPrecisionPointerForTesting != null) {
                return sSupportsPrecisionPointerForTesting;
            }
            for (int i = 0; i < mDeviceSnapshotsById.size(); i++) {
                if (mDeviceSnapshotsById.valueAt(i).supportsPrecisionPointer) {
                    return true;
                }
            }
            return false;
        }
    }

    /**
     * @return the Touchpad MotionRange of AXIS_X for the provided {@param deviceId}, or null if the
     *     device is not found or the device doesn't support touchpad source
     */
    public static InputDevice.@Nullable MotionRange getTouchpadXAxisMotionRange(int deviceId) {
        DeviceInput instance = getInstance();
        synchronized (instance.mLock) {
            DeviceSnapshot snapshot = instance.mDeviceSnapshotsById.get(deviceId);
            if (snapshot != null) {
                return snapshot.touchpadXAxisMotionRange;
            }
            return null;
        }
    }

    /**
     * @return the Touchpad MotionRange of AXIS_Y for the provided {@param deviceId}, or null if the
     *     device is not found or the device doesn't support touchpad source
     */
    public static InputDevice.@Nullable MotionRange getTouchpadYAxisMotionRange(int deviceId) {
        DeviceInput instance = getInstance();
        synchronized (instance.mLock) {
            DeviceSnapshot snapshot = instance.mDeviceSnapshotsById.get(deviceId);
            if (snapshot != null) {
                return snapshot.touchpadYAxisMotionRange;
            }
            return null;
        }
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        synchronized (mLock) {
            InputDevice device = InputDevice.getDevice(deviceId);
            if (device != null) {
                mDeviceSnapshotsById.put(deviceId, DeviceSnapshot.from(device));
            }
        }
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        synchronized (mLock) {
            InputDevice device = InputDevice.getDevice(deviceId);
            if (device != null) {
                mDeviceSnapshotsById.put(deviceId, DeviceSnapshot.from(device));
            } else {
                mDeviceSnapshotsById.remove(deviceId);
            }
        }
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        synchronized (mLock) {
            mDeviceSnapshotsById.remove(deviceId);
        }
    }

    /** Class which represents a snapshot of given {@link InputDevice}. */
    private static final class DeviceSnapshot {

        /** Whether the associated {@link InputDevice} supports an alphabetic keyboard. */
        public final boolean supportsAlphabeticKeyboard;

        /** Whether the associated {@link InputDevice} supports a keyboard. */
        public final boolean supportsKeyboard;

        /**
         * Whether the associated {@link InputDevice} supports precision pointing. Note that this
         * includes not only mice, but also any mice-like pointing devices (e.g. stylus, touchpad,
         * etc).
         */
        public final boolean supportsPrecisionPointer;

        /** The MotionRange of AXIS_X for the Touchpad source */
        public final InputDevice.MotionRange touchpadXAxisMotionRange;

        /** The MotionRange of AXIS_Y for the Touchpad source */
        public final InputDevice.MotionRange touchpadYAxisMotionRange;

        /** See {@link #from(InputDevice)}. */
        private DeviceSnapshot(
                boolean supportsAlphabeticKeyboard,
                boolean supportsPrecisionPointer,
                boolean supportsKeyboard,
                InputDevice.MotionRange touchpadXAxisMotionRange,
                InputDevice.MotionRange touchpadYAxisMotionRange) {
            this.supportsAlphabeticKeyboard = supportsAlphabeticKeyboard;
            this.supportsPrecisionPointer = supportsPrecisionPointer;
            this.touchpadXAxisMotionRange = touchpadXAxisMotionRange;
            this.touchpadYAxisMotionRange = touchpadYAxisMotionRange;
            this.supportsKeyboard = supportsKeyboard;
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
                            && device.supportsSource(SOURCE_MOUSE),
                    /* supportsKeyboard= */ isPhysical
                            && device.getKeyboardType() != KEYBOARD_TYPE_NONE,
                    device.getMotionRange(MotionEvent.AXIS_X, SOURCE_TOUCHPAD),
                    device.getMotionRange(MotionEvent.AXIS_Y, SOURCE_TOUCHPAD));
        }
    }
}

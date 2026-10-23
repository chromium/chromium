// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.events.devices;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.hardware.input.InputManager;
import android.hardware.input.InputManager.InputDeviceListener;
import android.view.InputDevice;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;

/**
 * A singleton that helps detecting changes in input devices through the interface {@link
 * InputDeviceObserver}.
 */
@JNINamespace("ui")
@NullMarked
public class InputDeviceObserver implements InputDeviceListener {
    private static final InputDeviceObserver INSTANCE = new InputDeviceObserver();

    /**
     * Notifies the InputDeviceObserver that an observer is attached and it should prepare itself
     * for listening input changes.
     */
    @CalledByNative
    public static void addObserver() {
        ThreadUtils.assertOnUiThread();
        INSTANCE.attachObserver();
    }

    /** Notifies the InputDeviceObserver that an observer has been removed. */
    @CalledByNative
    public static void removeObserver() {
        ThreadUtils.assertOnUiThread();
        INSTANCE.detachObserver();
    }

    private @Nullable InputManager mInputManager;
    private int mObserversCounter;

    // Override InputDeviceListener methods
    @Override
    public void onInputDeviceChanged(int deviceId) {
        ThreadUtils.assertOnUiThread();
        InputDeviceObserverJni.get().inputConfigurationChanged();
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        ThreadUtils.assertOnUiThread();
        InputDeviceObserverJni.get().inputConfigurationChanged();
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        ThreadUtils.assertOnUiThread();
        InputDeviceObserverJni.get().inputConfigurationChanged();
    }

    private void attachObserver() {
        ThreadUtils.assertOnUiThread();
        if (mObserversCounter++ == 0) {
            Context context = ContextUtils.getApplicationContext();
            mInputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
            // Register an input device listener.
            mInputManager.registerInputDeviceListener(this, null);
        }
    }

    private void detachObserver() {
        ThreadUtils.assertOnUiThread();
        assert mObserversCounter > 0;
        if (--mObserversCounter == 0) {
            assumeNonNull(mInputManager);
            mInputManager.unregisterInputDeviceListener(this);
            mInputManager = null;
        }
    }

    @CalledByNative
    public static InputDeviceData[] getInputDevices() {
        ThreadUtils.assertOnBackgroundThread();
        ArrayList<InputDeviceData> devices = new ArrayList<>();
        for (int deviceId : InputDevice.getDeviceIds()) {
            InputDevice device = getValidDevice(deviceId);
            if (device == null) continue;
            int sources = device.getSources();
            boolean isKeyboard =
                    (sources & InputDevice.SOURCE_KEYBOARD) == InputDevice.SOURCE_KEYBOARD
                            && device.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC;
            boolean isMouse = (sources & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE;
            boolean isTouchpad =
                    (sources & InputDevice.SOURCE_TOUCHPAD) == InputDevice.SOURCE_TOUCHPAD;
            boolean isTouchscreen =
                    (sources & InputDevice.SOURCE_TOUCHSCREEN) == InputDevice.SOURCE_TOUCHSCREEN;
            if (isKeyboard || isMouse || isTouchpad || isTouchscreen) {
                devices.add(
                        createDeviceData(device, isKeyboard, isMouse, isTouchpad, isTouchscreen));
            }
        }
        return devices.toArray(new InputDeviceData[0]);
    }

    private static @Nullable InputDevice getValidDevice(int deviceId) {
        assert deviceId < 1000000 : "Device ID " + deviceId + " exceeds 1M limit!";
        InputDevice device = null;
        try {
            device = InputDevice.getDevice(deviceId);
        } catch (RuntimeException e) {
            // Swallow the exception. See crbug.com/781377.
        }
        if (device == null || !device.isEnabled()) {
            return null;
        }
        return device;
    }

    private static InputDeviceData createDeviceData(
            InputDevice device,
            boolean isKeyboard,
            boolean isMouse,
            boolean isTouchpad,
            boolean isTouchscreen) {
        return new InputDeviceData(
                device.getId(),
                device.getName(),
                device.isExternal(),
                device.isVirtual(),
                device.getVendorId(),
                device.getProductId(),
                isKeyboard,
                isMouse,
                isTouchpad,
                isTouchscreen);
    }

    public static class InputDeviceData {
        private final int mId;
        private final String mName;
        private final boolean mIsExternal;
        private final boolean mIsVirtual;
        private final int mVendorId;
        private final int mProductId;
        private final boolean mIsKeyboard;
        private final boolean mIsMouse;
        private final boolean mIsTouchpad;
        private final boolean mIsTouchscreen;

        @CalledByNative
        public InputDeviceData(
                int id,
                String name,
                boolean isExternal,
                boolean isVirtual,
                int vendorId,
                int productId,
                boolean isKeyboard,
                boolean isMouse,
                boolean isTouchpad,
                boolean isTouchscreen) {
            mId = id;
            mName = name;
            mIsExternal = isExternal;
            mIsVirtual = isVirtual;
            mVendorId = vendorId;
            mProductId = productId;
            mIsKeyboard = isKeyboard;
            mIsMouse = isMouse;
            mIsTouchpad = isTouchpad;
            mIsTouchscreen = isTouchscreen;
        }

        @CalledByNative
        public int getId() {
            return mId;
        }

        @CalledByNative
        public String getName() {
            return mName;
        }

        @CalledByNative
        public boolean isExternal() {
            return mIsExternal;
        }

        @CalledByNative
        public boolean isVirtual() {
            return mIsVirtual;
        }

        @CalledByNative
        public int getVendorId() {
            return mVendorId;
        }

        @CalledByNative
        public int getProductId() {
            return mProductId;
        }

        @CalledByNative
        public boolean isKeyboard() {
            return mIsKeyboard;
        }

        @CalledByNative
        public boolean isMouse() {
            return mIsMouse;
        }

        @CalledByNative
        public boolean isTouchpad() {
            return mIsTouchpad;
        }

        @CalledByNative
        public boolean isTouchscreen() {
            return mIsTouchscreen;
        }
    }

    @NativeMethods
    interface Natives {
        void inputConfigurationChanged();
    }
}

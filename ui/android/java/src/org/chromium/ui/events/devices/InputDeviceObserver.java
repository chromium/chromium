// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.events.devices;

import android.content.Context;
import android.hardware.input.InputManager;
import android.hardware.input.InputManager.InputDeviceListener;
import android.util.ArrayMap;
import android.view.InputDevice;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;

/**
 * A singleton that helps detecting changes in input devices through the interface
 * {@link InputDeviceObserver}.
 */
@JNINamespace("ui")
public class InputDeviceObserver implements InputDeviceListener {
    private static final InputDeviceObserver INSTANCE = new InputDeviceObserver();
    private static final String KEYBOARD_CONNECTION_HISTOGRAM_NAME =
            "Android.InputDevice.Keyboard.Active";
    private static final String MOUSE_CONNECTION_HISTOGRAM_NAME =
            "Android.InputDevice.Mouse.Active";

    // Map to store the <deviceId, InputDevice.SOURCE*> information for an active/connected device.
    private final ArrayMap<Integer, Integer> mActiveDeviceMap = new ArrayMap<>();

    /**
     * Notifies the InputDeviceObserver that an observer is attached and it
     * should prepare itself for listening input changes.
     */
    @CalledByNative
    public static void addObserver() {
        assert ThreadUtils.runningOnUiThread();
        INSTANCE.attachObserver();
    }

    /** Notifies the InputDeviceObserver that an observer has been removed. */
    @CalledByNative
    public static void removeObserver() {
        assert ThreadUtils.runningOnUiThread();
        INSTANCE.detachObserver();
    }

    private InputManager mInputManager;
    private int mObserversCounter;

    // Override InputDeviceListener methods
    @Override
    public void onInputDeviceChanged(int deviceId) {
        InputDeviceObserverJni.get().inputConfigurationChanged(InputDeviceObserver.this);
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        InputDeviceObserverJni.get().inputConfigurationChanged(InputDeviceObserver.this);
        // InputDevice#getDevice() returns null for a removed device, and therefore we will use the
        // |mActiveDeviceMap| to determine the source type of the removed device.
        if (!mActiveDeviceMap.containsKey(deviceId)) return;
        if (mActiveDeviceMap.get(deviceId) == InputDevice.SOURCE_KEYBOARD) {
            RecordHistogram.recordBooleanHistogram(KEYBOARD_CONNECTION_HISTOGRAM_NAME, false);
        } else if (mActiveDeviceMap.get(deviceId) == InputDevice.SOURCE_MOUSE) {
            RecordHistogram.recordBooleanHistogram(MOUSE_CONNECTION_HISTOGRAM_NAME, false);
        }
        mActiveDeviceMap.remove(deviceId);
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        InputDeviceObserverJni.get().inputConfigurationChanged(InputDeviceObserver.this);
        var device = InputDevice.getDevice(deviceId);
        if (device == null) return;
        if ((device.getSources() & InputDevice.SOURCE_KEYBOARD) == InputDevice.SOURCE_KEYBOARD) {
            mActiveDeviceMap.put(deviceId, InputDevice.SOURCE_KEYBOARD);
            RecordHistogram.recordBooleanHistogram(KEYBOARD_CONNECTION_HISTOGRAM_NAME, true);
        } else if ((device.getSources() & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE) {
            mActiveDeviceMap.put(deviceId, InputDevice.SOURCE_MOUSE);
            RecordHistogram.recordBooleanHistogram(MOUSE_CONNECTION_HISTOGRAM_NAME, true);
        }
    }

    private void attachObserver() {
        if (mObserversCounter++ == 0) {
            Context context = ContextUtils.getApplicationContext();
            mInputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
            // Register an input device listener.
            mInputManager.registerInputDeviceListener(this, null);
        }
    }

    private void detachObserver() {
        assert mObserversCounter > 0;
        if (--mObserversCounter == 0) {
            mInputManager.unregisterInputDeviceListener(this);
            mInputManager = null;
        }
    }

    @NativeMethods
    interface Natives {
        void inputConfigurationChanged(InputDeviceObserver caller);
    }
}

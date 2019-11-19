// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.events.devices;

import android.content.Context;
import android.hardware.input.InputManager;
import android.hardware.input.InputManager.InputDeviceListener;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * A singleton that helps detecting changes in input devices through the interface
 * {@link InputDeviceObserver}.
 */
@JNINamespace("ui")
public class InputDeviceObserver implements InputDeviceListener {
    private static final InputDeviceObserver INSTANCE = new InputDeviceObserver();

    /**
     * Notifies the InputDeviceObserver that an observer is attached and it
     * should prepare itself for listening input changes.
     */
    @CalledByNative
    public static void addObserver() {
        assert ThreadUtils.runningOnUiThread();
        INSTANCE.attachObserver();
    }

    /**
     * Notifies the InputDeviceObserver that an observer has been removed.
     */
    @CalledByNative
    public static void removeObserver() {
        assert ThreadUtils.runningOnUiThread();
        INSTANCE.detachObserver();
    }

    private InputManager mInputManager;
    private InputDeviceListener mInputDeviceListener;
    private int mObserversCounter;

    // Override InputDeviceListener methods
    @Override
    public void onInputDeviceChanged(int deviceId) {
        InputDeviceObserverJni.get().inputConfigurationChanged(InputDeviceObserver.this);
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        InputDeviceObserverJni.get().inputConfigurationChanged(InputDeviceObserver.this);
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        InputDeviceObserverJni.get().inputConfigurationChanged(InputDeviceObserver.this);
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
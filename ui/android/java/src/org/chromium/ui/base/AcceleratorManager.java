// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.KeyEvent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Creates and owns a native ui::AcceleratorManager. */
@JNINamespace("ui")
@NullMarked
public class AcceleratorManager {
    private static final UnownedUserDataKey<AcceleratorManager> KEY = new UnownedUserDataKey<>();
    private final @Nullable LifetimeAssert mLifetimeAssert;
    // Note: the native side is not created until it's first accessed in
    // getNativePointerFromWindow().
    private long mNativeAcceleratorManagerAndroid;
    // False while no accelerators are registered. Accelerators can only be registered by native,
    // so if this is true, we know native is initialized.
    private boolean mAcceleratorsAreRegistered;

    /** Retrieve the AcceleratorManager from the WindowAndroid, creating it if it doesn't exist. */
    public static AcceleratorManager getOrCreate(WindowAndroid window) {
        AcceleratorManager manager = from(window);
        if (manager == null) {
            manager = new AcceleratorManager(window);
        }
        return manager;
    }

    public void destroy() {
        LifetimeAssert.destroy(mLifetimeAssert);
        if (mNativeAcceleratorManagerAndroid != 0) {
            AcceleratorManagerJni.get().destroy(mNativeAcceleratorManagerAndroid);
            mNativeAcceleratorManagerAndroid = 0;
            mAcceleratorsAreRegistered = false;
        }
    }

    private AcceleratorManager(WindowAndroid window) {
        mLifetimeAssert = LifetimeAssert.create(this);
        KEY.attachToHost(window.getUnownedUserDataHost(), this);
    }

    /**
     * Process a key event.
     *
     * @param event The key event to process.
     * @return True if the event was handled.
     */
    public boolean processKeyEvent(KeyEvent event) {
        return mNativeAcceleratorManagerAndroid != 0
                && mAcceleratorsAreRegistered
                && AcceleratorManagerJni.get()
                        .processKeyEvent(mNativeAcceleratorManagerAndroid, event);
    }

    /** Retrieve the AcceleratorManager from the WindowAndroid, if it exists. */
    private static @Nullable AcceleratorManager from(WindowAndroid window) {
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    @CalledByNative
    private static long getNativePointerFromWindow(WindowAndroid window) {
        AcceleratorManager manager = from(window);
        if (manager == null) {
            return 0;
        }
        return manager.getOrCreateNativePointer();
    }

    @CalledByNative
    private long getOrCreateNativePointer() {
        if (mNativeAcceleratorManagerAndroid == 0) {
            mNativeAcceleratorManagerAndroid = AcceleratorManagerJni.get().init(this);
        }
        return mNativeAcceleratorManagerAndroid;
    }

    // Notifies java whether accelerators are present, so that we can skip the JNI call for key
    // events if there are no accelerators present.
    @CalledByNative
    private void acceleratorsAreRegistered(boolean areRegistered) {
        mAcceleratorsAreRegistered = areRegistered;
    }

    @NativeMethods
    interface Natives {
        long init(AcceleratorManager caller);

        void destroy(long nativeAcceleratorManagerAndroid);

        boolean processKeyEvent(long nativeAcceleratorManagerAndroid, KeyEvent event);
    }
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.util.SparseArray;
import android.view.Display;
import android.view.WindowManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/**
 * DisplayAndroidManager is a class that informs its observers Display changes.
 */
@JNINamespace("ui")
@MainDex
public class DisplayAndroidManager {
    /**
     * DisplayListenerBackend is used to handle the actual listening of display changes. It handles
     * it via the Android DisplayListener API.
     */
    private class DisplayListenerBackend implements DisplayListener {
        public void startListening() {
            getDisplayManager().registerDisplayListener(this, null);
        }

        // DisplayListener implementation:

        @Override
        public void onDisplayAdded(int sdkDisplayId) {
            // DisplayAndroid is added lazily on first use. This is to workaround corner case
            // bug where DisplayManager.getDisplay(sdkDisplayId) returning null here.
        }

        @Override
        public void onDisplayRemoved(int sdkDisplayId) {
            // Never remove the primary display.
            if (sdkDisplayId == mMainSdkDisplayId) return;

            DisplayAndroid displayAndroid = mIdMap.get(sdkDisplayId);
            if (displayAndroid == null) return;

            if (mNativePointer != 0) {
                DisplayAndroidManagerJni.get().removeDisplay(
                        mNativePointer, DisplayAndroidManager.this, sdkDisplayId);
            }
            mIdMap.remove(sdkDisplayId);
        }

        @Override
        public void onDisplayChanged(int sdkDisplayId) {
            PhysicalDisplayAndroid displayAndroid =
                    (PhysicalDisplayAndroid) mIdMap.get(sdkDisplayId);
            Display display = getDisplayManager().getDisplay(sdkDisplayId);
            // Note display null check here is needed because there appear to be an edge case in
            // android display code, similar to onDisplayAdded.
            if (displayAndroid != null && display != null) {
                displayAndroid.updateFromDisplay(display);
            }
        }
    }

    private static DisplayAndroidManager sDisplayAndroidManager;

    // Real displays (as in, displays backed by an Android Display and recognized by the OS, though
    // not necessarily physical displays) on Android start at ID 0, and increment indefinitely as
    // displays are added. Display IDs are never reused until reboot. To avoid any overlap, start
    // virtual display ids at a much higher number, and increment them in the same way.
    private static final int VIRTUAL_DISPLAY_ID_BEGIN = Integer.MAX_VALUE / 2;

    private long mNativePointer;
    private int mMainSdkDisplayId;
    private final SparseArray<DisplayAndroid> mIdMap = new SparseArray<>();
    private DisplayListenerBackend mBackend = new DisplayListenerBackend();
    private int mNextVirtualDisplayId = VIRTUAL_DISPLAY_ID_BEGIN;

    /* package */ static DisplayAndroidManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sDisplayAndroidManager == null) {
            // Split between creation and initialization to allow for calls from DisplayAndroid to
            // reference sDisplayAndroidManager during initialize().
            sDisplayAndroidManager = new DisplayAndroidManager();
            sDisplayAndroidManager.initialize();
        }
        return sDisplayAndroidManager;
    }

    public static Display getDefaultDisplayForContext(Context context) {
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        return windowManager.getDefaultDisplay();
    }

    private static Context getContext() {
        // Using the global application context is probably ok.
        // The DisplayManager API observers all display updates, so in theory it should not matter
        // which context is used to obtain it. If this turns out not to be true in practice, it's
        // possible register from all context used though quite complex.
        return ContextUtils.getApplicationContext();
    }

    @SuppressLint("NewApi")
    private static DisplayManager getDisplayManager() {
        return (DisplayManager) getContext().getSystemService(Context.DISPLAY_SERVICE);
    }

    @CalledByNative
    private static void onNativeSideCreated(long nativePointer) {
        DisplayAndroidManager singleton = getInstance();
        singleton.setNativePointer(nativePointer);
    }

    private DisplayAndroidManager() {}

    private void initialize() {
        Display display;

        // Make sure the display map contains the built-in primary display.
        // The primary display is never removed.
        display = getDisplayManager().getDisplay(Display.DEFAULT_DISPLAY);

        // Android documentation on Display.DEFAULT_DISPLAY suggests that the above
        // method might return null. In that case we retrieve the default display
        // from the application context and take it as the primary display.
        if (display == null) display = getDefaultDisplayForContext(getContext());

        mMainSdkDisplayId = display.getDisplayId();
        addDisplay(display); // Note this display is never removed.

        mBackend.startListening();
    }

    private void setNativePointer(long nativePointer) {
        mNativePointer = nativePointer;
        DisplayAndroidManagerJni.get().setPrimaryDisplayId(
                mNativePointer, DisplayAndroidManager.this, mMainSdkDisplayId);

        for (int i = 0; i < mIdMap.size(); ++i) {
            updateDisplayOnNativeSide(mIdMap.valueAt(i));
        }
    }

    /* package */ DisplayAndroid getDisplayAndroid(Display display) {
        int sdkDisplayId = display.getDisplayId();
        DisplayAndroid displayAndroid = mIdMap.get(sdkDisplayId);
        if (displayAndroid == null) {
            displayAndroid = addDisplay(display);
        }
        return displayAndroid;
    }

    private DisplayAndroid addDisplay(Display display) {
        int sdkDisplayId = display.getDisplayId();
        PhysicalDisplayAndroid displayAndroid = new PhysicalDisplayAndroid(display);
        assert mIdMap.get(sdkDisplayId) == null;
        mIdMap.put(sdkDisplayId, displayAndroid);
        displayAndroid.updateFromDisplay(display);
        return displayAndroid;
    }

    private int getNextVirtualDisplayId() {
        return mNextVirtualDisplayId++;
    }

    /* package */ VirtualDisplayAndroid addVirtualDisplay() {
        VirtualDisplayAndroid display = new VirtualDisplayAndroid(getNextVirtualDisplayId());
        assert mIdMap.get(display.getDisplayId()) == null;
        mIdMap.put(display.getDisplayId(), display);
        updateDisplayOnNativeSide(display);
        return display;
    }

    /* package */ void removeVirtualDisplay(VirtualDisplayAndroid display) {
        DisplayAndroid displayAndroid = mIdMap.get(display.getDisplayId());
        assert displayAndroid == display;

        if (mNativePointer != 0) {
            DisplayAndroidManagerJni.get().removeDisplay(
                    mNativePointer, DisplayAndroidManager.this, display.getDisplayId());
        }
        mIdMap.remove(display.getDisplayId());
    }

    /* package */ void updateDisplayOnNativeSide(DisplayAndroid displayAndroid) {
        if (mNativePointer == 0) return;
        DisplayAndroidManagerJni.get().updateDisplay(mNativePointer, DisplayAndroidManager.this,
                displayAndroid.getDisplayId(), displayAndroid.getDisplayWidth(),
                displayAndroid.getDisplayHeight(), displayAndroid.getDipScale(),
                displayAndroid.getRotationDegrees(), displayAndroid.getBitsPerPixel(),
                displayAndroid.getBitsPerComponent(), displayAndroid.getIsWideColorGamut());
    }

    @NativeMethods
    interface Natives {
        void updateDisplay(long nativeDisplayAndroidManager, DisplayAndroidManager caller,
                int sdkDisplayId, int width, int height, float dipScale, int rotationDegrees,
                int bitsPerPixel, int bitsPerComponent, boolean isWideColorGamut);
        void removeDisplay(
                long nativeDisplayAndroidManager, DisplayAndroidManager caller, int sdkDisplayId);
        void setPrimaryDisplayId(
                long nativeDisplayAndroidManager, DisplayAndroidManager caller, int sdkDisplayId);
    }
}

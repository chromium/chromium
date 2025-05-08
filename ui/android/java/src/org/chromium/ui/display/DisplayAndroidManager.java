// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Handler;
import android.os.Looper;
import android.util.SparseArray;
import android.view.Display;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.UiAndroidFeatureList;

import java.util.HashSet;

/** DisplayAndroidManager is a class that informs its observers Display changes. */
@JNINamespace("ui")
@NullMarked
public class DisplayAndroidManager {
    /**
     * DisplayListenerBackend is used to handle the actual listening of display changes. It handles
     * it via the Android DisplayListener API.
     */
    @VisibleForTesting
    class DisplayListenerBackend implements DisplayListener {
        private static final long IS_NULL_DISPLAY_REMOVED_DELAY_MS = 1000;

        @VisibleForTesting
        static final String IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME =
                "Android.Display.IsNullDisplayRemoved";

        private final Handler mHandler = new Handler(Looper.getMainLooper());
        private final HashSet<Integer> mNullDisplayIds = new HashSet<Integer>();

        public void startListening() {
            getDisplayManager().registerDisplayListener(this, null);
        }

        // DisplayListener implementation:

        @Override
        public void onDisplayAdded(int sdkDisplayId) {
            if (!UiAndroidFeatureList.sAndroidWindowManagementWebApi.isEnabled()) {
                return;
            }

            Display display = getDisplayManager().getDisplay(sdkDisplayId);
            if (display != null) {
                addDisplay(display);
                return;
            }

            mNullDisplayIds.add(sdkDisplayId);
            mHandler.postDelayed(
                    () -> {
                        // Record whether sdkDisplayId was still in mNullDisplayIds at
                        // this point. This indicates if an onDisplayRemoved call for
                        // sdkDisplayId did not occur within
                        // IS_NULL_DISPLAY_REMOVED_DELAY_MS
                        // after its corresponding onDisplayAdded.
                        // - true: sdkDisplayId was already removed; onDisplayRemoved
                        // occurred and processed it.
                        // - false: sdkDisplayId was present and removed now;
                        // onDisplayRemoved was missed or late.
                        RecordHistogram.recordBooleanHistogram(
                                IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME,
                                !mNullDisplayIds.remove(sdkDisplayId));
                    },
                    IS_NULL_DISPLAY_REMOVED_DELAY_MS);
        }

        @Override
        public void onDisplayRemoved(int sdkDisplayId) {
            if (UiAndroidFeatureList.sAndroidWindowManagementWebApi.isEnabled()) {
                mNullDisplayIds.remove(sdkDisplayId);
            }

            // Never remove the primary display.
            if (sdkDisplayId == mMainSdkDisplayId) return;

            PhysicalDisplayAndroid displayAndroid =
                    (PhysicalDisplayAndroid) mIdMap.get(sdkDisplayId);
            if (displayAndroid == null) return;

            displayAndroid.onDisplayRemoved();
            if (mNativePointer != 0) {
                DisplayAndroidManagerJni.get()
                        .removeDisplay(mNativePointer, DisplayAndroidManager.this, sdkDisplayId);
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

    private static @Nullable DisplayAndroidManager sDisplayAndroidManager;

    private static boolean sDisableHdrSdkRatioCallback;

    private long mNativePointer;
    private int mMainSdkDisplayId;
    @VisibleForTesting final SparseArray<DisplayAndroid> mIdMap = new SparseArray<>();
    @VisibleForTesting final DisplayListenerBackend mBackend = new DisplayListenerBackend();

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

    // Disable hdr/sdr ratio callback. Ratio will always be reported as 1.
    public static void disableHdrSdrRatioCallback() {
        sDisableHdrSdkRatioCallback = true;
    }

    public static Display getDefaultDisplayForContext(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Display display = null;
            try {
                display = context.getDisplay();
            } catch (UnsupportedOperationException e) {
                // Context is not associated with a display.
            }
            if (display != null) return display;
            return getGlobalDefaultDisplay();
        }
        return getDisplayForContextNoChecks(context);
    }

    private static Display getGlobalDefaultDisplay() {
        return getDisplayManager().getDisplay(Display.DEFAULT_DISPLAY);
    }

    // Passing a non-window display may cause problems on newer android versions.
    private static Display getDisplayForContextNoChecks(Context context) {
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
        // Make sure the display map contains the built-in primary display.
        // The primary display is never removed.
        Display defaultDisplay = getGlobalDefaultDisplay();

        // Android documentation on Display.DEFAULT_DISPLAY suggests that the above
        // method might return null. In that case we retrieve the default display
        // from the application context and take it as the primary display.
        if (defaultDisplay == null) {
            defaultDisplay = getDisplayForContextNoChecks(getContext());
        }

        mMainSdkDisplayId = defaultDisplay.getDisplayId(); // Note this display is never removed.

        if (UiAndroidFeatureList.sAndroidWindowManagementWebApi.isEnabled()) {
            for (Display display : getDisplayManager().getDisplays()) {
                addDisplay(display);
            }
        } else {
            addDisplay(defaultDisplay);
        }

        mBackend.startListening();
    }

    private void setNativePointer(long nativePointer) {
        mNativePointer = nativePointer;
        DisplayAndroidManagerJni.get()
                .setPrimaryDisplayId(mNativePointer, DisplayAndroidManager.this, mMainSdkDisplayId);

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
        PhysicalDisplayAndroid displayAndroid =
                new PhysicalDisplayAndroid(display, sDisableHdrSdkRatioCallback);
        assert mIdMap.get(sdkDisplayId) == null;
        mIdMap.put(sdkDisplayId, displayAndroid);
        displayAndroid.updateFromDisplay(display);
        return displayAndroid;
    }

    /* package */ void updateDisplayOnNativeSide(DisplayAndroid displayAndroid) {
        if (mNativePointer == 0) return;

        int[] insetsArray = new int[] {0, 0, 0, 0};
        if (VERSION.SDK_INT >= VERSION_CODES.R) {
            insetsArray = displayAndroid.getInsetsAsArray();
        }

        DisplayAndroidManagerJni.get()
                .updateDisplay(
                        mNativePointer,
                        DisplayAndroidManager.this,
                        displayAndroid.getDisplayId(),
                        displayAndroid.getDisplayName(),
                        displayAndroid.getBoundsAsArray(),
                        insetsArray,
                        displayAndroid.getDipScale(),
                        displayAndroid.getRotationDegrees(),
                        displayAndroid.getBitsPerPixel(),
                        displayAndroid.getBitsPerComponent(),
                        displayAndroid.getIsWideColorGamut(),
                        displayAndroid.getIsHdr(),
                        displayAndroid.getHdrMaxLuminanceRatio(),
                        displayAndroid.isInternal());
    }

    @NativeMethods
    interface Natives {
        void updateDisplay(
                long nativeDisplayAndroidManager,
                DisplayAndroidManager caller,
                int sdkDisplayId,
                @Nullable String label,
                int[] bounds, // the order is: left, top, right, bottom
                int[] insets, // the order is: left, top, right, bottom
                float dipScale,
                int rotationDegrees,
                int bitsPerPixel,
                int bitsPerComponent,
                boolean isWideColorGamut,
                boolean isHdr,
                float hdrMaxLuminanceRatio,
                boolean isInternal);

        void removeDisplay(
                long nativeDisplayAndroidManager, DisplayAndroidManager caller, int sdkDisplayId);

        void setPrimaryDisplayId(
                long nativeDisplayAndroidManager, DisplayAndroidManager caller, int sdkDisplayId);
    }

    /** Clears the object returned by {@link #getInstance()} */
    public static void resetInstanceForTesting() {
        sDisplayAndroidManager = null;
    }
}

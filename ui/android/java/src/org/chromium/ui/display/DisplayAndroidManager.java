// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.SparseArray;
import android.view.Display;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
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
    private static final String TAG = "DisplayManager";

    /**
     * DisplayListenerBackend is used to handle the actual listening of display changes. It handles
     * it via the Android DisplayListener API.
     */
    @VisibleForTesting
    class DisplayListenerBackend implements DisplayListener {
        public void startListening() {
            getDisplayManager().registerDisplayListener(this, null);
        }

        // DisplayListener implementation:
        @Override
        public void onDisplayAdded(int sdkDisplayId) {
            // Ignore display addition if Display Topology is available. The addition is processed
            // inside {@link DisplayAndroidManager#updateDisplayTopology(SparseArray<RectF>
            // newDisplaysAbsoluteCoordinates)} when {@link
            // DisplayTopologyListenerBackend#onDisplayTopologyChanged(SparseArray<RectF>
            // absoluteBounds)} is triggered.
            // If Display Topology is not available, then DisplayAndroid is added lazily on first
            // use.
        }

        @Override
        public void onDisplayRemoved(int sdkDisplayId) {
            // Ignore display removal if Display Topology is available. The removal is processed
            // inside {@link DisplayAndroidManager#updateDisplayTopology(SparseArray<RectF>
            // newDisplaysAbsoluteCoordinates)} when {@link
            // DisplayTopologyListenerBackend#onDisplayTopologyChanged(SparseArray<RectF>
            // absoluteBounds)} is triggered.
            if (!isDisplayTopologyAvailable()) {
                removeDisplay(sdkDisplayId);
            }
        }

        @Override
        public void onDisplayChanged(int sdkDisplayId) {
            updateDisplay(sdkDisplayId);
        }
    }

    /**
     * DisplayTopologyListenerBackend is used to handle the actual listening of display topology
     * changes. It handles it via the Android Display Manager API.
     */
    class DisplayTopologyListenerBackend
            implements AconfigFlaggedApiDelegate.DisplayTopologyListener {
        public void startListening() {
            assumeNonNull(AconfigFlaggedApiDelegate.getInstance())
                    .registerTopologyListener(
                            getDisplayManager(), getContext().getMainExecutor(), this);
        }

        // AconfigFlaggedApiDelegate.DisplayTopologyListener implementation:
        @Override
        public void onDisplayTopologyChanged(SparseArray<RectF> absoluteBounds) {
            updateDisplayTopology(absoluteBounds);
        }
    }

    private static @Nullable DisplayAndroidManager sDisplayAndroidManager;

    private static boolean sDisableHdrSdkRatioCallback;
    private static @Nullable Boolean sIsDisplayTopologyAvailable;

    private static final long IS_NULL_DISPLAY_REMOVED_DELAY_MS = 1000;

    @VisibleForTesting
    static final String IS_DISPLAY_TOPOLOGY_AVAILABLE_HISTOGRAM_NAME =
            "Android.Display.IsDisplayTopologyAvaialble";

    @VisibleForTesting
    static final String IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME =
            "Android.Display.IsNullDisplayRemoved";

    private long mNativePointer;
    private int mMainSdkDisplayId;
    @VisibleForTesting final SparseArray<DisplayAndroid> mIdMap = new SparseArray<>();
    private final DisplayListenerBackend mBackend = new DisplayListenerBackend();

    private final HashSet<Integer> mNullDisplayIds = new HashSet<>();
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    @VisibleForTesting @Nullable DisplayTopologyListenerBackend mDisplayTopologyListenerBackend;
    private @Nullable SparseArray<RectF> mDisplaysAbsoluteCoordinates;

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

    @CalledByNative
    public static boolean isDisplayTopologyAvailable() {
        if (sIsDisplayTopologyAvailable == null) {
            sIsDisplayTopologyAvailable =
                    UiAndroidFeatureList.sAndroidUseDisplayTopology.isEnabled()
                            && AconfigFlaggedApiDelegate.getInstance() != null
                            && AconfigFlaggedApiDelegate.getInstance()
                                    .isDisplayTopologyAvailable(getDisplayManager());
        }
        return sIsDisplayTopologyAvailable;
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

    private static DisplayManager getDisplayManager() {
        return (DisplayManager) getContext().getSystemService(Context.DISPLAY_SERVICE);
    }

    @CalledByNative
    private static void onNativeSideCreated(long nativePointer) {
        final DisplayAndroidManager singleton = getInstance();

        // In Browser Tests CommandLine is overridden when the Java DisplayAndroidManager has
        // already been created and initialized, so we should update ForcedDIPScale to synchronize
        // the two states.
        if (PhysicalDisplayAndroid.isForcedDIPScaleChanged()) {
            for (int i = 0; i < singleton.mIdMap.size(); ++i) {
                final DisplayAndroid display = singleton.mIdMap.valueAt(i);
                if (display instanceof PhysicalDisplayAndroid) {
                    singleton.updateDisplay(display.getDisplayId());
                }
            }
        }

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

        Log.i(TAG, "Is Display Topology available: " + isDisplayTopologyAvailable());
        RecordHistogram.recordBooleanHistogram(
                IS_DISPLAY_TOPOLOGY_AVAILABLE_HISTOGRAM_NAME, isDisplayTopologyAvailable());

        if (isDisplayTopologyAvailable()) {
            mDisplaysAbsoluteCoordinates =
                    assumeNonNull(
                            assumeNonNull(AconfigFlaggedApiDelegate.getInstance())
                                    .getAbsoluteBounds(getDisplayManager()));
            for (int i = 0; i < mDisplaysAbsoluteCoordinates.size(); ++i) {
                int sdkDisplayId = mDisplaysAbsoluteCoordinates.keyAt(i);
                addDisplayById(sdkDisplayId, mDisplaysAbsoluteCoordinates.valueAt(i));
            }

            mDisplayTopologyListenerBackend = new DisplayTopologyListenerBackend();
            mDisplayTopologyListenerBackend.startListening();
        } else {
            addDisplay(defaultDisplay, null);
        }

        mBackend.startListening();
    }

    private void setNativePointer(long nativePointer) {
        mNativePointer = nativePointer;
        DisplayAndroidManagerJni.get().setPrimaryDisplayId(mNativePointer, mMainSdkDisplayId);

        for (int i = 0; i < mIdMap.size(); ++i) {
            updateDisplayOnNativeSide(mIdMap.valueAt(i));
        }
    }

    /* package */ DisplayAndroid getDisplayAndroid(Display display) {
        int sdkDisplayId = display.getDisplayId();
        DisplayAndroid displayAndroid = mIdMap.get(sdkDisplayId);
        if (displayAndroid == null) {
            displayAndroid = addDisplay(display, null);
        }
        return displayAndroid;
    }

    private DisplayAndroid addDisplay(Display display, @Nullable RectF displayAbsoluteCoordinates) {
        int sdkDisplayId = display.getDisplayId();
        PhysicalDisplayAndroid displayAndroid =
                new PhysicalDisplayAndroid(
                        display, displayAbsoluteCoordinates, sDisableHdrSdkRatioCallback);
        assert mIdMap.get(sdkDisplayId) == null;
        mIdMap.put(sdkDisplayId, displayAndroid);
        return displayAndroid;
    }

    private void addDisplayById(int sdkDisplayId, RectF displayAbsoluteCoordinates) {
        Display display = getDisplayManager().getDisplay(sdkDisplayId);
        if (display != null) {
            addDisplay(display, displayAbsoluteCoordinates);
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

    private void removeDisplay(int sdkDisplayId) {
        if (isDisplayTopologyAvailable()) {
            mNullDisplayIds.remove(sdkDisplayId);
        }

        // Never remove the primary display.
        if (sdkDisplayId == mMainSdkDisplayId) return;

        PhysicalDisplayAndroid displayAndroid = (PhysicalDisplayAndroid) mIdMap.get(sdkDisplayId);
        if (displayAndroid == null) return;

        displayAndroid.onDisplayRemoved();
        if (mNativePointer != 0) {
            DisplayAndroidManagerJni.get().removeDisplay(mNativePointer, sdkDisplayId);
        }
        mIdMap.remove(sdkDisplayId);
    }

    private void updateDisplay(int sdkDisplayId) {
        PhysicalDisplayAndroid displayAndroid = (PhysicalDisplayAndroid) mIdMap.get(sdkDisplayId);
        Display display = getDisplayManager().getDisplay(sdkDisplayId);

        // Note display null check here is needed because there appear to be an edge case in
        // android display code, similar to onDisplayAdded.
        if (displayAndroid != null && display != null) {
            displayAndroid.updateFromDisplay(display);
        }
    }

    @SuppressLint("NewApi")
    private void updateDisplayTopology(SparseArray<RectF> newDisplaysAbsoluteCoordinates) {
        assumeNonNull(mDisplaysAbsoluteCoordinates);

        for (int i = 0; i < newDisplaysAbsoluteCoordinates.size(); ++i) {
            int sdkDisplayId = newDisplaysAbsoluteCoordinates.keyAt(i);
            RectF displayAbsoluteCoordinates = mDisplaysAbsoluteCoordinates.get(sdkDisplayId);
            RectF newDisplayAbsoluteCoordinates = newDisplaysAbsoluteCoordinates.valueAt(i);

            if (displayAbsoluteCoordinates == null) {
                addDisplayById(sdkDisplayId, newDisplayAbsoluteCoordinates);
            } else if (!displayAbsoluteCoordinates.equals(newDisplayAbsoluteCoordinates)) {
                assumeNonNull((PhysicalDisplayAndroid) mIdMap.get(sdkDisplayId))
                        .updateBounds(newDisplayAbsoluteCoordinates);
            }
        }

        for (int i = 0; i < mDisplaysAbsoluteCoordinates.size(); ++i) {
            int sdkDisplayId = mDisplaysAbsoluteCoordinates.keyAt(i);
            if (!newDisplaysAbsoluteCoordinates.contains(sdkDisplayId)) {
                removeDisplay(sdkDisplayId);
            }
        }

        mDisplaysAbsoluteCoordinates = newDisplaysAbsoluteCoordinates;
    }

    /* package */ void updateDisplayOnNativeSide(DisplayAndroid displayAndroid) {
        if (mNativePointer == 0) return;

        DisplayAndroidManagerJni.get()
                .updateDisplay(
                        mNativePointer,
                        displayAndroid.getDisplayId(),
                        displayAndroid.getDisplayName(),
                        displayAndroid.getBoundsAsArray(),
                        displayAndroid.getWorkAreaAsArray(),
                        displayAndroid.getDisplayWidth(),
                        displayAndroid.getDisplayHeight(),
                        displayAndroid.getDipScale(),
                        displayAndroid.getXdpi(),
                        displayAndroid.getYdpi(),
                        displayAndroid.getRotationDegrees(),
                        displayAndroid.getBitsPerPixel(),
                        displayAndroid.getBitsPerComponent(),
                        displayAndroid.getIsWideColorGamut(),
                        displayAndroid.getIsHdr(),
                        displayAndroid.getHdrMaxLuminanceRatio(),
                        displayAndroid.isInternal());
    }

    /**
     * Matches the given rectangle in dip to the display it most closely intersects.
     *
     * @param matchRect Area in dip that should be matched.
     * @return {@link DisplayAndroid} that most closely intersects the given rectangle, or {@code
     *     null} if no matching display is found.
     */
    @VisibleForTesting
    public @Nullable DisplayAndroid getDisplayMatching(Rect matchRect) {
        if (mNativePointer == 0) {
            return null;
        }

        int sdkDisplayId =
                DisplayAndroidManagerJni.get()
                        .getDisplaySdkMatching(
                                mNativePointer,
                                matchRect.left,
                                matchRect.top,
                                matchRect.width(),
                                matchRect.height());
        return mIdMap.get(sdkDisplayId);
    }

    @NativeMethods
    interface Natives {
        void updateDisplay(
                long nativeDisplayAndroidManager,
                int sdkDisplayId,
                @Nullable String label,
                int[] bounds, // {left, top, right, bottom} in dip
                int[] workArea, // {left, top, right, bottom} in dip
                int wight, // in physical pixels
                int height, // in physical pixels
                float dipScale,
                float xDpi,
                float yDpi,
                int rotationDegrees,
                int bitsPerPixel,
                int bitsPerComponent,
                boolean isWideColorGamut,
                boolean isHdr,
                float hdrMaxLuminanceRatio,
                boolean isInternal);

        void removeDisplay(long nativeDisplayAndroidManager, int sdkDisplayId);

        void setPrimaryDisplayId(long nativeDisplayAndroidManager, int sdkDisplayId);

        int getDisplaySdkMatching(
                long nativeDisplayAndroidManager, int x, int y, int width, int height);
    }

    public static void setInstanceForTesting(DisplayAndroidManager displayAndroidManager) {
        sDisplayAndroidManager = displayAndroidManager;
        ResettersForTesting.register(() -> resetInstanceForTesting()); // IN-TEST
    }

    /** Clears the object returned by {@link #getInstance()} */
    public static void resetInstanceForTesting() {
        sDisplayAndroidManager = null;
    }

    public static void resetIsDisplayTopologyAvailableForTesting() {
        sIsDisplayTopologyAvailable = null;
    }
}

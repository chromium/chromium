// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.AttachedSurfaceControl;
import android.view.FrameMetrics;
import android.view.SurfaceControl;
import android.view.Window;

import androidx.annotation.RequiresApi;

import org.chromium.ui.gfx.OverlayTransform;

import java.lang.ref.WeakReference;

/** Helper class to avoid fail of ART's class verification for S_V2 APIs in old device. */
@RequiresApi(Build.VERSION_CODES.S_V2)
final class OverlayTransformApiHelper
        implements AttachedSurfaceControl.OnBufferTransformHintChangedListener,
                Window.OnFrameMetricsAvailableListener {
    private final WindowAndroid mWindowAndroid;
    private final WeakReference<Window> mWindow;
    private boolean mBufferTransformListenerAdded;
    private boolean mFrameMetricsListenerAdded;

    static OverlayTransformApiHelper create(WindowAndroid windowAndroid) {
        if (windowAndroid.getWindow() == null) return null;
        return new OverlayTransformApiHelper(windowAndroid);
    }

    private OverlayTransformApiHelper(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        mWindow = new WeakReference<>(mWindowAndroid.getWindow());
        addOnBufferTransformHintChangedListener();
    }

    void destroy() {
        removeOnFrameMetricsAvailableListener();
        removeOnBufferTransformHintChangedListener();
    }

    private void addOnBufferTransformHintChangedListener() {
        Window window = mWindow.get();
        if (window == null) return;
        AttachedSurfaceControl surfacecontrol = window.getRootSurfaceControl();
        if (surfacecontrol == null) {
            // If AttachedSurfaceControl is not available yet, wait until it's ready and set the
            // listener.
            addOnFrameMetricsAvailableListener();
        } else {
            doAddOnBufferTransformHintChangedListener();
        }
    }

    @Override
    public void onBufferTransformHintChanged(int hint) {
        mWindowAndroid.onOverlayTransformUpdated();
    }

    private void doAddOnBufferTransformHintChangedListener() {
        if (mBufferTransformListenerAdded) return;
        Window window = mWindow.get();
        if (window == null) return;
        AttachedSurfaceControl surfacecontrol = window.getRootSurfaceControl();
        if (surfacecontrol != null) {
            surfacecontrol.addOnBufferTransformHintChangedListener(this);
            mBufferTransformListenerAdded = true;
        }
    }

    private void removeOnBufferTransformHintChangedListener() {
        if (!mBufferTransformListenerAdded) return;

        Window window = mWindow.get();
        if (window == null) return;
        AttachedSurfaceControl surfacecontrol = window.getRootSurfaceControl();
        if (surfacecontrol != null) {
            surfacecontrol.removeOnBufferTransformHintChangedListener(this);
            mBufferTransformListenerAdded = false;
        }
    }

    @Override
    public void onFrameMetricsAvailable(Window window, FrameMetrics frameMetrics, int dropCount) {
        // AttachedSurfaceControl is available after setContentView is called and 1st draw happen.
        AttachedSurfaceControl surfaceControl = window.getRootSurfaceControl();
        if (surfaceControl != null) {
            removeOnFrameMetricsAvailableListener();
            doAddOnBufferTransformHintChangedListener();
        }
    }

    private void addOnFrameMetricsAvailableListener() {
        if (mFrameMetricsListenerAdded) return;
        Window window = mWindow.get();
        if (window == null) return;
        window.addOnFrameMetricsAvailableListener(this, new Handler(Looper.myLooper()));
        mFrameMetricsListenerAdded = true;
    }

    private void removeOnFrameMetricsAvailableListener() {
        if (!mFrameMetricsListenerAdded) return;
        Window window = mWindow.get();
        if (window == null) return;
        window.removeOnFrameMetricsAvailableListener(this);
        mFrameMetricsListenerAdded = false;
    }

    @OverlayTransform
    int getOverlayTransform() {
        Window window = mWindow.get();
        if (window == null) return OverlayTransform.INVALID;
        AttachedSurfaceControl surfacecontrol = window.getRootSurfaceControl();
        if (surfacecontrol == null) {
            return OverlayTransform.INVALID;
        }
        int bufferTransform;
        try {
            bufferTransform = surfacecontrol.getBufferTransformHint();
        } catch (NullPointerException | IllegalStateException e) {
            // Can throw exception from implementation of getBufferTransformHint.
            // See crbug.com/1358898, crbug.com/1468179.
            return OverlayTransform.INVALID;
        }
        return toOverlayTransform(bufferTransform);
    }

    private static @OverlayTransform int toOverlayTransform(int bufferTransform) {
        switch (bufferTransform) {
            case SurfaceControl.BUFFER_TRANSFORM_IDENTITY:
                return OverlayTransform.NONE;
            case SurfaceControl.BUFFER_TRANSFORM_MIRROR_HORIZONTAL:
                return OverlayTransform.FLIP_HORIZONTAL;
            case SurfaceControl.BUFFER_TRANSFORM_MIRROR_VERTICAL:
                return OverlayTransform.FLIP_VERTICAL;
            case SurfaceControl.BUFFER_TRANSFORM_ROTATE_90:
                return OverlayTransform.ROTATE_CLOCKWISE_90;
            case SurfaceControl.BUFFER_TRANSFORM_ROTATE_180:
                return OverlayTransform.ROTATE_CLOCKWISE_180;
            case SurfaceControl.BUFFER_TRANSFORM_ROTATE_270:
                return OverlayTransform.ROTATE_CLOCKWISE_270;
                // Combination cases between BUFFER_TRANSFORM_MIRROR_HORIZONTAL,
                // BUFFER_TRANSFORM_MIRROR_VERTICAL, BUFFER_TRANSFORM_ROTATE_90 are not handled
                // because expected behavior is under-specified by android APIs
            default:
                // INVALID makes WindowAndroid fallback to display rotation
                return OverlayTransform.INVALID;
        }
    }
}

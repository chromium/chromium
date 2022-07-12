// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceFactory;

/**
 * An adapter that exposes a {@link View} as a {@link DynamicResourceSnapshot}. In order to properly
 * use this adapter {@link ViewResourceAdapter#invalidate(Rect)} must be called when parts of the
 * {@link View} are invalidated.  For {@link ViewGroup}s the easiest way to do this is to override
 * {@link ViewGroup#invalidateChildInParent(int[], Rect)}.
 */
public class ViewResourceAdapter
        implements DynamicResource, OnLayoutChangeListener, CaptureObserver {
    /** A plain object to hold the return values from {@link CaptureMechanism#syncCaptureBitmap}. */
    public static class CaptureResult {
        public final Bitmap bitmap;
        public final boolean clearDirtyRect;

        /**
         * @param bitmap The drawn pixels for the view being captured.
         * @param clearDirtyRect If the dirty rect can be cleared, because a capture happened.
         */
        public CaptureResult(Bitmap bitmap, boolean clearDirtyRect) {
            this.bitmap = bitmap;
            this.clearDirtyRect = clearDirtyRect;
        }
    }

    /** Abstraction around the mechanism for actually capturing bitmaps.  */
    public interface CaptureMechanism {
        /** See {@link Resource#shouldRemoveResourceOnNullBitmap()}. */
        boolean shouldRemoveResourceOnNullBitmap();
        /** If a capture should be taken based on state of the capture mechanism. */
        boolean shouldPretendIsDirty();
        /** Called when the size of the view changes. */
        void onViewSizeChange(View view, float scale);
        /** Called to drop any cached bitmaps to free up memory. */
        void dropCachedBitmap();
        /**
         * Called to trigger the actual bitmap capture.
         * @param view The view being captured.
         * @param dirtyRect The area that has changed since last capture.
         * @param scale Scalar to apply to width and height when capturing a bitmap.
         * @param observer To be notified before and after the capture happens.
         * @return The result of the capture.
         */
        CaptureResult syncCaptureBitmap(
                View view, Rect dirtyRect, float scale, CaptureObserver observer);
    }

    private final View mView;
    private final Rect mDirtyRect = new Rect();
    private final Rect mViewSize = new Rect();
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();
    private final CaptureMechanism mCaptureMechanism;
    private float mScale = 1;

    @Nullable
    private Callback<Resource> mOnResourceReady;

    /**
     * Builds a {@link ViewResourceAdapter} instance around {@code view}.
     * @param view The {@link View} to expose as a {@link Resource}.
     *
     * @param useHardwareBitmapDraw controls if we should software draw bitmaps or use a
     * RenderNode and hardware acceleration.
     */
    @SuppressWarnings("NewApi")
    public ViewResourceAdapter(View view, boolean useHardwareBitmapDraw) {
        mView = view;

        // It is possible the view has not had an layout pass yet, and these values are wrong. Even
        // when this is the case, because we also listen to onLayout changes, we will update
        // accordingly.
        mView.addOnLayoutChangeListener(this);
        mViewSize.set(0, 0, mView.getWidth(), mView.getHeight());
        mDirtyRect.set(mViewSize);

        // Enforce hardware accelerated drawing on android Q+ where it's supported.
        useHardwareBitmapDraw &= Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
        if (useHardwareBitmapDraw) {
            mCaptureMechanism = new HardwareDraw();
        } else {
            mCaptureMechanism = new SoftwareDraw();
        }
    }

    /**
     * Builds a {@link ViewResourceAdapter} instance around {@code view}.
     * @param view The {@link View} to expose as a {@link Resource}.
     */
    public ViewResourceAdapter(View view) {
        this(view, false);
    }

    /**
     * Typically called when ({@link #isDirty()} returned {@code true}), to return a new
     * {@link Bitmap} and clear out the dirty rect, resulting in a non-dirty view. Depending on the
     * draw mechanism, this may return a null bitmap. In such a case, on the next frame, isDirty()
     * should still be used to decide whether to call this.
     * @return A {@link Bitmap} representing the {@link View}.
     */
    @SuppressWarnings("NewApi")
    public Bitmap getBitmap() {
        mThreadChecker.assertOnValidThread();
        try (TraceEvent e = TraceEvent.scoped("ViewResourceAdapter:getBitmap")) {
            CaptureResult result =
                    mCaptureMechanism.syncCaptureBitmap(mView, new Rect(mDirtyRect), mScale, this);
            if (result.clearDirtyRect) {
                mDirtyRect.setEmpty();
            }
            return result.bitmap;
        }
    }

    /**
     * Set the downsampling scale. The rendered size is not affected.
     * @param scale The scale to use. <1 means the Bitmap is smaller than the View.
     */
    public void setDownsamplingScale(float scale) {
        assert scale <= 1;
        if (mScale != scale) {
            invalidate(null);
        }
        mScale = scale;
    }

    /** {@see Resource#createNativeResource()}. */
    public long createNativeResource() {
        return ResourceFactory.createBitmapResource(null);
    }

    @Override
    public void setOnResourceReadyCallback(Callback<Resource> onResourceReady) {
        mOnResourceReady = onResourceReady;
    }

    /**
     * On every render frame, this resources will check to see if it is dirty. If so, it will take
     * a bitmap capture and push it to the renderer. Subclasses can override this method to suppress
     * when captures are/can be taken. Although this will likely run into problems with hardware
     * draws because the bitmap they return lags behind by a capture. Should ultimately be fixed as
     * part of https://crbug.com/1338202.
     * @return If a bitmap capture should be taken/started.
     */
    @CallSuper
    protected boolean isDirty() {
        // The bitmap is dirty if some part of it has changed, or the capture mode wants to return a
        // new bitmap.
        return !mDirtyRect.isEmpty() || mCaptureMechanism.shouldPretendIsDirty();
    }

    @Override
    public void onResourceRequested() {
        // TODO(skym): The hardware capture approach should be pushing bitmaps when they're ready,
        // and avoid relying on isDirty and/or onResourceRequested signals. However this is an
        // intermediate state during refactoring, and is intentionally keeping the old behavior for
        // now.
        if (mOnResourceReady != null && isDirty()) {
            Bitmap bitmap = getBitmap();
            boolean removeOnNull = mCaptureMechanism.shouldRemoveResourceOnNullBitmap();
            Resource resource = new DynamicResourceSnapshot(
                    bitmap, removeOnNull, mViewSize, createNativeResource());
            mOnResourceReady.onResult(resource);
        }
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        final int width = right - left;
        final int height = bottom - top;
        final int oldWidth = oldRight - oldLeft;
        final int oldHeight = oldBottom - oldTop;

        if (width != oldWidth || height != oldHeight) {
            mViewSize.set(0, 0, width, height);
            mDirtyRect.set(0, 0, width, height);
            mCaptureMechanism.onViewSizeChange(mView, mScale);
        }
    }

    /**
     * Invalidates a particular region of the {@link View} that needs to be repainted.
     * @param dirtyRect The region to invalidate, or {@code null} if the entire {@code Bitmap}
     *                  should be redrawn.
     */
    public void invalidate(Rect dirtyRect) {
        if (dirtyRect == null) {
            mDirtyRect.set(0, 0, mView.getWidth(), mView.getHeight());
        } else {
            mDirtyRect.union(dirtyRect);
        }
    }

    /** Drops the cached bitmap to free up memory. */
    public void dropCachedBitmap() {
        mCaptureMechanism.dropCachedBitmap();
    }

    /** Returns the dirty rect that will be drawn on capture. */
    @VisibleForTesting
    protected Rect getDirtyRect() {
        return mDirtyRect;
    }
}

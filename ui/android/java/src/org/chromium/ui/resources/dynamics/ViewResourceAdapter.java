// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
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
    /** Abstraction around the mechanism for actually capturing bitmaps.  */
    public interface CaptureMechanism {
        /** See {@link Resource#shouldRemoveResourceOnNullBitmap()}. */
        boolean shouldRemoveResourceOnNullBitmap();

        /** Called when the size of the view changes. */
        default void onViewSizeChange(View view, float scale) {}

        /** Called to drop any cached bitmaps to free up memory. */
        void dropCachedBitmap();

        /**
         * Called to trigger the actual bitmap capture.
         *
         * @param view The view being captured.
         * @param dirtyRect The area that has changed since last capture.
         * @param scale Scalar to apply to width and height when capturing a bitmap.
         * @param observer To be notified before and after the capture happens.
         * @param onBitmapCapture The callback to return the recorded image.
         * @return If the dirty rect can be cleared on a successful capture.
         */
        boolean startBitmapCapture(
                View view,
                Rect dirtyRect,
                float scale,
                CaptureObserver observer,
                Callback<Bitmap> onBitmapCapture);
    }

    private final View mView;
    private final Rect mDirtyRect = new Rect();
    private final Rect mViewSize = new Rect();
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();
    private final CaptureMechanism mCaptureMechanism;
    private float mScale = 1;

    private final ObserverList<Callback<Resource>> mOnResourceReadyObservers = new ObserverList<>();

    /**
     * Builds a {@link ViewResourceAdapter} instance around {@code view}.
     *
     * @param view The {@link View} to expose as a {@link Resource}.
     */
    @SuppressWarnings("NewApi")
    public ViewResourceAdapter(View view) {
        mView = view;

        // It is possible the view has not had an layout pass yet, and these values are wrong. Even
        // when this is the case, because we also listen to onLayout changes, we will update
        // accordingly.
        mView.addOnLayoutChangeListener(this);
        mViewSize.set(0, 0, mView.getWidth(), mView.getHeight());
        mDirtyRect.set(mViewSize);

        mCaptureMechanism = new SoftwareDraw();
    }

    /**
     * Triggers a bitmap capture ignoring whether the view is dirty. Depending on this mechanism, it
     * may do some or all of the work, and may be sync or async.
     */
    @SuppressWarnings("NewApi")
    public void triggerBitmapCapture() {
        mThreadChecker.assertOnValidThread();
        try (TraceEvent e = TraceEvent.scoped("ViewResourceAdapter:getBitmap")) {
            if (mCaptureMechanism.startBitmapCapture(
                    mView, new Rect(mDirtyRect), mScale, this, this::onCapture)) {
                mDirtyRect.setEmpty();
            }
        }
    }

    private void onCapture(Bitmap bitmap) {
        mThreadChecker.assertOnValidThread();
        Resource resource =
                new DynamicResourceSnapshot(
                        bitmap,
                        mCaptureMechanism.shouldRemoveResourceOnNullBitmap(),
                        mViewSize,
                        createNativeResource());
        for (Callback<Resource> observer : mOnResourceReadyObservers) observer.onResult(resource);
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
    public void addOnResourceReadyCallback(Callback<Resource> onResourceReady) {
        mOnResourceReadyObservers.addObserver(onResourceReady);
    }

    @Override
    public void removeOnResourceReadyCallback(Callback<Resource> onResourceReady) {
        mOnResourceReadyObservers.removeObserver(onResourceReady);
    }

    /**
     * On every render frame, this resources will check to see if it is dirty. If so, it will take
     * a bitmap capture and push it to the renderer. Subclasses can override this method to suppress
     * when captures are/can be taken.
     * @return If a bitmap capture should be taken/started.
     */
    @CallSuper
    protected boolean isDirty() {
        return !mDirtyRect.isEmpty();
    }

    @Override
    public void onResourceRequested() {
        if (!mOnResourceReadyObservers.isEmpty() && isDirty()) {
            triggerBitmapCapture();
        }
    }

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
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
    public Rect getDirtyRect() {
        return mDirtyRect;
    }

    /** Clears the contents of the current dirty rect. */
    protected void setDirtyRectEmpty() {
        mDirtyRect.setEmpty();
    }
}

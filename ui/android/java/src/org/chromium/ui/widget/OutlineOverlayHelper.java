// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.chromium.build.NullUtil.assertNonNull;

import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.View.OnFocusChangeListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;

/**
 * A helper class to draw a focus outline for a {@link View} using a {@link
 * android.view.ViewOverlay}. The outline is shown when the host view has focus and hidden
 * otherwise.
 */
@NullMarked
public class OutlineOverlayHelper implements OnFocusChangeListener, OnLayoutChangeListener {
    private static final int OVERLAY_TAG = R.id.outline_overlay_drawable;

    private final View mHost;
    private final Drawable mOutlineDrawable;
    private final int mOutlineOffsetPx;

    private final Rect mBounds = new Rect();

    // Null until mHost is attached to the window.
    private @Nullable ViewGroup mParent;
    boolean mIsOutlineAttached;

    /**
     * Attach an instance of OutlineOverlayHelper to the input host view, and use {@link
     * View#getParent()} to retrieve the parent view to attach the view overlay. The default focus
     * outline drawable that has corner radius of 16dp will be used.
     *
     * <p>The OutlineOverlayHelper will be attached when the view is attached to the window.
     *
     * @param host View that needs to be outlined.
     * @see #attach(View, Drawable)
     */
    public static OutlineOverlayHelper attach(View host) {
        Drawable outlineDrawable =
                assertNonNull(
                        AppCompatResources.getDrawable(
                                host.getContext(),
                                R.drawable.focused_outline_overlay_corners_16dp));
        outlineDrawable.mutate();
        return attach(host, outlineDrawable);
    }

    /**
     * Attach an instance of OutlineOverlayHelper to the input host view, and use {@link
     * View#getParent()} to retrieve the parent view to attach the view overlay.
     *
     * <p>The OutlineOverlayHelper will be attached when the view is attached to the window.
     *
     * @param host View that needs to be outlined.
     * @param outlineDrawable The outline drawable to use when focused.
     */
    public static OutlineOverlayHelper attach(View host, Drawable outlineDrawable) {
        Connector connector = new Connector(host, outlineDrawable);
        return connector.mInstance;
    }

    /**
     * Set the stroke color for the outline drawable. Implemented using a {@link
     * PorterDuffColorFilter}.
     *
     * @param color The color of the stroke.
     */
    public void setStrokeColor(int color) {
        mOutlineDrawable.setColorFilter(new PorterDuffColorFilter(color, Mode.SRC_IN));
    }

    /**
     * Create an instance to draw outline for the host view using ViewOverlay.
     *
     * @param host The view to draw the outline for.
     * @param parent The parent of the host view. The overlay will be added to this parent.
     * @param outlineDrawable The drawable to use for the outline.
     */
    OutlineOverlayHelper(View host, @Nullable ViewGroup parent, Drawable outlineDrawable) {
        mHost = host;
        mParent = parent;
        mOutlineDrawable = outlineDrawable;
        mOutlineOffsetPx =
                mHost.getResources().getDimensionPixelSize(R.dimen.focused_outline_offset);

        // Suppress the host View's default system focus highlight to avoid conflicts.
        mHost.setDefaultFocusHighlightEnabled(false);
        mHost.setOnFocusChangeListener(this);
        mHost.addOnLayoutChangeListener(this);
        mHost.post(this::updateOutline);
    }

    @Override
    public void onFocusChange(View v, boolean hasFocus) {
        updateOutline();
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oLeft,
            int oTop,
            int oRight,
            int oBottom) {
        updateOutline();
    }

    /** Cleans up the listeners and the overlay. */
    void destroy() {
        mIsOutlineAttached = false;
        mHost.setOnFocusChangeListener(null);
        mHost.removeOnLayoutChangeListener(this);
        if (mParent != null) {
            mParent.getOverlay().remove(mOutlineDrawable);
            mParent = null;
        }
    }

    private void updateOutline() {
        if (mParent == null) return;

        // Calculate the expanded bounds for the outline, incorporating the offset.
        // These bounds are relative to the parent's overlay.
        Rect expandedBounds =
                new Rect(
                        mHost.getLeft() - mOutlineOffsetPx,
                        mHost.getTop() - mOutlineOffsetPx,
                        mHost.getRight() + mOutlineOffsetPx,
                        mHost.getBottom() + mOutlineOffsetPx);

        if (expandedBounds.equals(mBounds) && mIsOutlineAttached == mHost.hasFocus()) {
            return;
        }

        // Remove overlay if the host was previously focused. This ensures the drawable will
        // not be added more than once.
        if (mIsOutlineAttached) {
            mParent.getOverlay().remove(mOutlineDrawable);
            mHost.setTag(OVERLAY_TAG, null);
        }

        mIsOutlineAttached = mHost.hasFocus();

        if (mIsOutlineAttached) {
            mBounds.set(expandedBounds);
            mOutlineDrawable.setBounds(mBounds);
            mOutlineDrawable.setState(mHost.getDrawableState());

            mParent.getOverlay().add(mOutlineDrawable);
            mHost.setTag(OVERLAY_TAG, mOutlineDrawable);
        } else {
            mBounds.setEmpty();
        }

        // Invalidate the drawable to follow the new bounds.
        mOutlineDrawable.invalidateSelf();
    }

    boolean isOutlineAttachedForTesting() {
        return mIsOutlineAttached;
    }

    /** Get the overlay drawable for the host. */
    public static @Nullable Drawable getFocusOutlineDrawableForTesting(View hostView) {
        Object overlay = hostView.getTag(R.id.outline_overlay_drawable);
        return overlay instanceof Drawable ? (Drawable) overlay : null;
    }

    // Helper class that is used to attach OutlineOverlayHelper to the parent. This is needed
    // for cases where view is created during run time / as part of a list adapter.
    private static class Connector implements OnAttachStateChangeListener {
        private final View mHost;
        private final OutlineOverlayHelper mInstance;

        Connector(View host, Drawable outlineDrawable) {
            mHost = host;
            mHost.addOnAttachStateChangeListener(this);
            mInstance =
                    new OutlineOverlayHelper(mHost, (ViewGroup) mHost.getParent(), outlineDrawable);
        }

        @Override
        public void onViewAttachedToWindow(View view) {
            assert mHost.getParent() instanceof ViewGroup;
            if (mInstance.mParent == null) {
                mInstance.mParent = (ViewGroup) mHost.getParent();
                mInstance.updateOutline();
            }
        }

        @Override
        public void onViewDetachedFromWindow(View view) {
            mInstance.destroy();
            view.removeOnAttachStateChangeListener(this);
        }
    }
}

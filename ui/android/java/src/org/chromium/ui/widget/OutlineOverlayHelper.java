// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.R;

/**
 * A helper class to draw a focus outline for a {@link View} using a {@link
 * android.view.ViewOverlay}. The outline is shown when the host view has focus and hidden
 * otherwise. The user of this class is responsible for calling {@link #destroy()} when the host
 * view is no longer needed to avoid memory leaks.
 */
@NullMarked
public class OutlineOverlayHelper implements OnFocusChangeListener, OnLayoutChangeListener {
    private final View mHost;
    private final ViewGroup mParent;
    private final Drawable mOutlineDrawable;
    private final int mOutlineOffsetPx;

    private final Rect mBounds = new Rect();

    boolean mIsOutlineAttached;

    /**
     * Create an instance to draw outline for the host view using ViewOverlay.
     *
     * @param host The view to draw the outline for.
     * @param parent The parent of the host view. The overlay will be added to this parent.
     * @param outlineDrawable The drawable to use for the outline.
     */
    public OutlineOverlayHelper(View host, ViewGroup parent, Drawable outlineDrawable) {
        mHost = host;
        mParent = parent;
        mOutlineDrawable = outlineDrawable;
        mOutlineOffsetPx =
                mHost.getResources().getDimensionPixelSize(R.dimen.focused_outline_offset);

        // Suppress the host View's default system focus highlight to avoid conflicts.
        mHost.setDefaultFocusHighlightEnabled(false);

        // Attach listeners to the host View for focus and layout changes.
        mHost.setOnFocusChangeListener(this);
        mHost.addOnLayoutChangeListener(this);

        // Perform initial update in case the host View starts focused or layout is already done.
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
    public void destroy() {
        mHost.setOnFocusChangeListener(null);
        mHost.removeOnLayoutChangeListener(this);
        mParent.getOverlay().remove(mOutlineDrawable);
    }

    private void updateOutline() {
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
        }

        mIsOutlineAttached = mHost.hasFocus();

        if (mIsOutlineAttached) {
            mBounds.set(expandedBounds);
            mOutlineDrawable.setBounds(mBounds);
            mOutlineDrawable.setState(mHost.getDrawableState());

            mParent.getOverlay().add(mOutlineDrawable);
        } else {
            mBounds.setEmpty();
        }

        // Invalidate the parent to trigger a redraw of the outline Drawable.
        mOutlineDrawable.invalidateSelf();
    }

    boolean isOutlineAttachedForTesting() {
        return mIsOutlineAttached;
    }
}

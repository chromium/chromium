// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Rect;
import android.view.View;

import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.ViewRectProvider.ViewRectUpdateStrategy;
import org.chromium.ui.widget.ViewRectProvider.ViewRectUpdateStrategyFactory;

import java.util.Objects;

/**
 * The default implementation of {@link ViewRectUpdateStrategy} that calculates and updates a {@link
 * Rect} based on the position and dimensions of a specified {@link View} within its window.
 *
 * <p>This updater uses {@link View#getLocationInWindow(int[])} to determine the view's position. It
 * allows for configuration options such as:
 *
 * <ul>
 *   <li>Applying {@link Rect} insets (subtracted from bounds).
 *   <li>Applying {@link Rect} margins (added to bounds).
 *   <li>Including or excluding the view's padding from the final Rect.
 *   <li>Calculating the center point instead of the full bounds.
 * </ul>
 */
@NullMarked
public class ViewRectUpdater implements ViewRectUpdateStrategy {
    private final int[] mCachedWindowCoordinates = new int[2];
    private final View mView;
    private final Rect mRect;
    private final Runnable mOnRectChanged;
    private final Rect mInsetRect = new Rect();
    private final Rect mMarginRect = new Rect();

    private int mCachedViewWidth = -1;
    private int mCachedViewHeight = -1;

    private boolean mIncludePadding;
    private boolean mUseCenterPoint;

    /**
     * Creates an instance of a {@link ViewRectUpdater}. See {@link
     * ViewRectUpdateStrategyFactory#create(View, Rect, Runnable)}.
     *
     * @param view The {@link View} whose bounds will be tracked.
     * @param rect The {@link Rect} instance that will be updated by this class with the view's
     *     calculated bounds. This object is modified directly.
     * @param onRectChanged A {@link Runnable} that will be executed whenever the |rect| parameter
     *     is updated.
     */
    public ViewRectUpdater(View view, Rect rect, Runnable onRectChanged) {
        mView = view;
        mRect = rect;
        mOnRectChanged = onRectChanged;
        mCachedWindowCoordinates[0] = -1;
        mCachedWindowCoordinates[1] = -1;
    }

    @Override
    public void refreshRectBounds(boolean forceRefresh) {
        int previousPositionX = mCachedWindowCoordinates[0];
        int previousPositionY = mCachedWindowCoordinates[1];
        int previousWidth = mCachedViewWidth;
        int previousHeight = mCachedViewHeight;
        mView.getLocationInWindow(mCachedWindowCoordinates);

        mCachedWindowCoordinates[0] = Math.max(mCachedWindowCoordinates[0], 0);
        mCachedWindowCoordinates[1] = Math.max(mCachedWindowCoordinates[1], 0);
        mCachedViewWidth = mView.getWidth();
        mCachedViewHeight = mView.getHeight();

        // Return if the window coordinates and view sizes haven't changed.
        if (!forceRefresh
                && mCachedWindowCoordinates[0] == previousPositionX
                && mCachedWindowCoordinates[1] == previousPositionY
                && mCachedViewWidth == previousWidth
                && mCachedViewHeight == previousHeight) {
            return;
        }

        mRect.left = mCachedWindowCoordinates[0];
        mRect.top = mCachedWindowCoordinates[1];
        mRect.right = mRect.left + mView.getWidth();
        mRect.bottom = mRect.top + mView.getHeight();

        mRect.left += mInsetRect.left;
        mRect.top += mInsetRect.top;
        mRect.right -= mInsetRect.right;
        mRect.bottom -= mInsetRect.bottom;

        mRect.left -= mMarginRect.left;
        mRect.top -= mMarginRect.top;
        mRect.right += mMarginRect.right;
        mRect.bottom += mMarginRect.bottom;

        // Account for the padding.
        if (!mIncludePadding) {
            boolean isRtl = mView.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
            mRect.left +=
                    isRtl ? ViewCompat.getPaddingEnd(mView) : ViewCompat.getPaddingStart(mView);
            mRect.right -=
                    isRtl ? ViewCompat.getPaddingStart(mView) : ViewCompat.getPaddingEnd(mView);
            mRect.top += mView.getPaddingTop();
            mRect.bottom -= mView.getPaddingBottom();
        }

        // Make sure we still have a valid Rect after applying the inset.
        mRect.right = Math.max(mRect.left, mRect.right);
        mRect.bottom = Math.max(mRect.top, mRect.bottom);

        mRect.right = Math.min(mRect.right, mView.getRootView().getWidth());
        mRect.bottom = Math.min(mRect.bottom, mView.getRootView().getHeight());

        if (mUseCenterPoint) {
            int centerX = mRect.left + mRect.width() / 2;
            int centerY = mRect.top + mRect.height() / 2;
            mRect.set(centerX, centerY, centerX, centerY);
        }
        mOnRectChanged.run();
    }

    @Override
    public void setInsetPx(Rect insetRect) {
        if (Objects.equals(mInsetRect, insetRect)) return;
        mInsetRect.set(insetRect);

        refreshRectBounds(/* forceRefresh= */ true);
    }

    @Override
    public void setMarginPx(Rect marginRect) {
        if (Objects.equals(mInsetRect, marginRect)) return;

        mMarginRect.set(marginRect);
        refreshRectBounds(/* forceRefresh= */ true);
    }

    @Override
    public void setIncludePadding(boolean includePadding) {
        if (includePadding == mIncludePadding) return;

        mIncludePadding = includePadding;
        refreshRectBounds(/* forceRefresh= */ true);
    }

    @Override
    public void setUseCenter(boolean useCenterPoint) {
        mUseCenterPoint = useCenterPoint;
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.core.view.ViewCompat;

/**
 * Provides a {@Rect} for the location of a {@View} in its window, see
 * {@link View#getLocationOnScreen(int[])}.
 */
public class ViewRectProvider extends RectProvider
        implements ViewTreeObserver.OnGlobalLayoutListener, View.OnAttachStateChangeListener,
                   ViewTreeObserver.OnPreDrawListener {
    private final int[] mCachedWindowCoordinates = new int[2];
    private final Rect mInsetRect = new Rect();
    private final View mView;

    /** If not {@code null}, the {@link ViewTreeObserver} that we are registered to. */
    private ViewTreeObserver mViewTreeObserver;

    private boolean mIncludePadding;

    /**
     * Creates an instance of a {@link ViewRectProvider}.
     * @param view The {@link View} used to generate a {@link Rect}.
     */
    public ViewRectProvider(View view) {
        mView = view;
        mCachedWindowCoordinates[0] = -1;
        mCachedWindowCoordinates[1] = -1;
    }

    /**
     * Specifies the inset values in pixels that determine how to shrink the {@link View} bounds
     * when creating the {@link Rect}.
     */
    public void setInsetPx(int left, int top, int right, int bottom) {
        mInsetRect.set(left, top, right, bottom);
        refreshRectBounds();
    }

    /**
     * Specifies the inset values in pixels that determine how to shrink the {@link View} bounds
     * when creating the {@link Rect}.
     */
    public void setInsetPx(Rect insetRect) {
        mInsetRect.set(insetRect);
        refreshRectBounds();
    }

    /**
     * Whether padding should be included in the {@link Rect} for the {@link View}.
     * @param includePadding Whether padding should be included. Defaults to false.
     */
    public void setIncludePadding(boolean includePadding) {
        mIncludePadding = includePadding;
    }

    @Override
    public void startObserving(Observer observer) {
        mView.addOnAttachStateChangeListener(this);
        mViewTreeObserver = mView.getViewTreeObserver();
        mViewTreeObserver.addOnGlobalLayoutListener(this);
        mViewTreeObserver.addOnPreDrawListener(this);

        refreshRectBounds();

        super.startObserving(observer);
    }

    @Override
    public void stopObserving() {
        mView.removeOnAttachStateChangeListener(this);

        if (mViewTreeObserver != null && mViewTreeObserver.isAlive()) {
            mViewTreeObserver.removeOnGlobalLayoutListener(this);
            mViewTreeObserver.removeOnPreDrawListener(this);
        }
        mViewTreeObserver = null;

        super.stopObserving();
    }

    // ViewTreeObserver.OnGlobalLayoutListener implementation.
    @Override
    public void onGlobalLayout() {
        if (!mView.isShown()) notifyRectHidden();
    }

    // ViewTreeObserver.OnPreDrawListener implementation.
    @Override
    public boolean onPreDraw() {
        if (!mView.isShown()) {
            notifyRectHidden();
        } else {
            refreshRectBounds();
        }

        return true;
    }

    // View.OnAttachStateChangedObserver implementation.
    @Override
    public void onViewAttachedToWindow(View v) {}

    @Override
    public void onViewDetachedFromWindow(View v) {
        notifyRectHidden();
    }

    private void refreshRectBounds() {
        int previousPositionX = mCachedWindowCoordinates[0];
        int previousPositionY = mCachedWindowCoordinates[1];
        mView.getLocationInWindow(mCachedWindowCoordinates);

        mCachedWindowCoordinates[0] = Math.max(mCachedWindowCoordinates[0], 0);
        mCachedWindowCoordinates[1] = Math.max(mCachedWindowCoordinates[1], 0);

        // Return if the window coordinates haven't changed.
        if (mCachedWindowCoordinates[0] == previousPositionX
                && mCachedWindowCoordinates[1] == previousPositionY) {
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

        notifyRectChanged();
    }
}

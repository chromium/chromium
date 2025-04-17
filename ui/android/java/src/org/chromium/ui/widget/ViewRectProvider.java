// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Provides a {@link Rect} for the location of a {@link View} in its window, see {@link
 * View#getLocationOnScreen(int[])}. When view bound changes, {@link RectProvider.Observer} will be
 * notified.
 */
@NullMarked
public class ViewRectProvider extends RectProvider
        implements ViewTreeObserver.OnGlobalLayoutListener,
                View.OnAttachStateChangeListener,
                ViewTreeObserver.OnPreDrawListener {
    /** The strategy for calculating the {@link Rect} bounds based on a {@link View}. */
    public interface ViewRectUpdateStrategy {
        /**
         * Recalculates the view's bounds based on its current position, dimensions, and the
         * configured inset, margin, padding, and center point settings.
         *
         * @param forceRefresh Whether the rect bounds should be refreshed even when the window
         *     coordinates and view sizes haven't changed. This is needed when inset or padding
         *     changes.
         */
        void refreshRectBounds(boolean forceRefresh);

        /**
         * Specifies the inset values in pixels that determine how to shrink the {@link View} bounds
         * when creating the {@link Rect}.
         */
        default void setInsetPx(Rect insetRect) {}

        /**
         * Specifies the margin values in pixels that determine how to expand the {@link View}
         * bounds when creating the {@link Rect}.
         */
        default void setMarginPx(Rect marginRect) {}

        /**
         * Whether padding should be included in the {@link Rect} for the {@link View}.
         *
         * @param includePadding Whether padding should be included. Defaults to false.
         */
        default void setIncludePadding(boolean includePadding) {}

        /**
         * Whether use the center of the view after all the adjustment applied (insets, margins).
         * The Rect being provided will be a single point.
         *
         * @param useCenterPoint Whether the rect represents the center of the view after
         *     adjustments.
         */
        default void setUseCenter(boolean useCenterPoint) {}
    }

    /** A factory for creating instances of {@link ViewRectUpdateStrategy}. */
    @FunctionalInterface
    public interface ViewRectUpdateStrategyFactory {
        /**
         * @param view The {@link View} whose bounds will be tracked.
         * @param rect The {@link Rect} instance that will be updated by this class with the view's
         *     calculated bounds. This object is modified directly.
         * @param onRectChanged A {@link Runnable} that will be executed whenever the |rect|
         *     parameter is updated.
         */
        ViewRectUpdateStrategy create(View view, Rect rect, Runnable onRectChanged);
    }

    private final View mView;
    private final ViewRectUpdateStrategy mUpdateStrategy;

    /** If not {@code null}, the {@link ViewTreeObserver} that we are registered to. */
    private @Nullable ViewTreeObserver mViewTreeObserver;

    /**
     * Creates an instance of a {@link ViewRectProvider}.
     *
     * @param view The {@link View} used to generate a {@link Rect}.
     */
    public ViewRectProvider(View view) {
        this(view, ViewRectUpdater::new);
    }

    public ViewRectProvider(View view, ViewRectUpdateStrategyFactory factory) {
        mView = view;
        mUpdateStrategy = factory.create(view, mRect, this::notifyRectChanged);
    }

    /**
     * Specifies the inset values in pixels that determine how to shrink the {@link View} bounds
     * when creating the {@link Rect}.
     */
    public void setInsetPx(int left, int top, int right, int bottom) {
        setInsetPx(new Rect(left, top, right, bottom));
    }

    /** See {@link ViewRectUpdateStrategy#setInsetPx(Rect)}. */
    public void setInsetPx(Rect insetRect) {
        mUpdateStrategy.setInsetPx(insetRect);
    }

    /**
     * Specifies the margin values in pixels that determine how to expand the {@link View} bounds
     * when creating the {@link Rect}.
     */
    public void setMarginPx(int left, int top, int right, int bottom) {
        setMarginPx(new Rect(left, top, right, bottom));
    }

    /** See {@link ViewRectUpdateStrategy#setMarginPx(Rect)}. */
    public void setMarginPx(Rect marginRect) {
        mUpdateStrategy.setMarginPx(marginRect);
    }

    /** See {@link ViewRectUpdateStrategy#setIncludePadding(boolean)}. */
    public void setIncludePadding(boolean includePadding) {
        mUpdateStrategy.setIncludePadding(includePadding);
    }

    /** See {@link ViewRectUpdateStrategy#setUseCenter(boolean)}. */
    public void setUseCenter(boolean useCenterPoint) {
        mUpdateStrategy.setUseCenter(useCenterPoint);
    }

    @Override
    public void startObserving(Observer observer) {
        mView.addOnAttachStateChangeListener(this);
        mViewTreeObserver = mView.getViewTreeObserver();
        mViewTreeObserver.addOnGlobalLayoutListener(this);
        mViewTreeObserver.addOnPreDrawListener(this);

        refreshRectBounds(/* forceRefresh= */ false);

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
            refreshRectBounds(/* forceRefresh= */ false);
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

    /**
     * @param forceRefresh Whether the rect bounds should be refreshed even when the window
     *     coordinates and view sizes haven't changed. This is needed when inset or padding changes.
     */
    private void refreshRectBounds(boolean forceRefresh) {
        mUpdateStrategy.refreshRectBounds(forceRefresh);
    }

    public View getViewForTesting() {
        return mView;
    }
}

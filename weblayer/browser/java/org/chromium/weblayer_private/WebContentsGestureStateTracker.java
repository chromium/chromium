// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.view.MotionEvent;
import android.view.View;

import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;

/**
 * WebContentsGestureStateTracker is responsible for tracking when a scroll/gesture is in progress
 * and notifying when the state changes.
 */
// TODO(sky): refactor TabGestureStateListener and this to a common place.
public final class WebContentsGestureStateTracker {
    private GestureListenerManager mGestureListenerManager;
    private GestureStateListener mGestureListener;
    private final OnGestureStateChangedListener mListener;
    private boolean mScrolling;
    private boolean mIsInGesture;

    /**
     * The View events are tracked on.
     */
    private View mContentView;

    /**
     * Notified when the gesture state changes.
     */
    public interface OnGestureStateChangedListener {
        /**
         * Called when the value of isInGestureOrScroll() changes.
         */
        public void onGestureStateChanged();
    }

    public WebContentsGestureStateTracker(
            View contentView, WebContents webContents, OnGestureStateChangedListener listener) {
        mListener = listener;
        mGestureListenerManager = GestureListenerManager.fromWebContents(webContents);
        mContentView = contentView;
        mContentView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent event) {
                final int eventAction = event.getActionMasked();
                final boolean oldState = isInGestureOrScroll();
                if (eventAction == MotionEvent.ACTION_DOWN
                        || eventAction == MotionEvent.ACTION_POINTER_DOWN) {
                    mIsInGesture = true;
                } else if (eventAction == MotionEvent.ACTION_CANCEL
                        || eventAction == MotionEvent.ACTION_UP) {
                    mIsInGesture = false;
                }
                if (isInGestureOrScroll() != oldState) {
                    mListener.onGestureStateChanged();
                }
                return false;
            }
        });

        mGestureListener = new GestureStateListener() {
            @Override
            public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            private void onScrollingStateChanged() {
                final boolean oldState = isInGestureOrScroll();
                mScrolling = mGestureListenerManager.isScrollInProgress();
                if (oldState != isInGestureOrScroll()) {
                    mListener.onGestureStateChanged();
                }
            }
        };
        mGestureListenerManager.addListener(mGestureListener);
    }

    public void destroy() {
        mGestureListenerManager.removeListener(mGestureListener);
        mGestureListener = null;
        mGestureListenerManager = null;
        mContentView.setOnTouchListener(null);
    }

    /**
     * Returns true if the user has touched the target view, or is scrolling.
     */
    public boolean isInGestureOrScroll() {
        return mIsInGesture || mScrolling;
    }
}

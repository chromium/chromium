// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.DragEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

/**
 * A class to update motion event offset while dragging. This is needed to compensate the change
 * caused by top control.
 */
public class EventOffsetHandler {
    /**
     * A delegate for EventOffsetHandler.
     */
    public interface EventOffsetHandlerDelegate {
        float getTop();
        void setCurrentTouchEventOffsets(float top);
    }

    private final EventOffsetHandlerDelegate mDelegate;

    public EventOffsetHandler(EventOffsetHandlerDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Call this before handling onDispatchDragEvent.
     * @param action Drag event action.
     */
    public void onPreDispatchDragEvent(int action) {
        setTouchEventOffsets(-mDelegate.getTop());
    }

    /**
     * Call this after handling onDispatchDragEvent.
     * @param action Drag event action.
     */
    public void onPostDispatchDragEvent(int action) {
        if (action == DragEvent.ACTION_DRAG_EXITED || action == DragEvent.ACTION_DRAG_ENDED
                || action == DragEvent.ACTION_DROP) {
            setTouchEventOffsets(0.f);
        }
    }

    /** See {@link ViewGroup#onInterceptTouchEvent(MotionEvent)}. */
    public void onInterceptTouchEvent(MotionEvent e) {
        setContentViewMotionEventOffsets(e, false);
    }

    /** See {@link View#onTouchEvent(MotionEvent)}. */
    public void onTouchEvent(MotionEvent e) {
        setContentViewMotionEventOffsets(e, true);
    }

    /** See {@link ViewGroup#onInterceptHoverEvent(MotionEvent)}. */
    public void onInterceptHoverEvent(MotionEvent e) {
        setContentViewMotionEventOffsets(e, true);
    }

    private void setContentViewMotionEventOffsets(MotionEvent e, boolean canClear) {
        int actionMasked = SPenSupport.convertSPenEventAction(e.getActionMasked());
        if (actionMasked == MotionEvent.ACTION_DOWN
                || actionMasked == MotionEvent.ACTION_HOVER_ENTER
                || actionMasked == MotionEvent.ACTION_HOVER_MOVE) {
            setTouchEventOffsets(-mDelegate.getTop());
        } else if (canClear
                && (actionMasked == MotionEvent.ACTION_UP
                        || actionMasked == MotionEvent.ACTION_CANCEL
                        || actionMasked == MotionEvent.ACTION_HOVER_EXIT)) {
            setTouchEventOffsets(0.f);
        }
    }

    private void setTouchEventOffsets(float y) {
        mDelegate.setCurrentTouchEventOffsets(y);
    }
}

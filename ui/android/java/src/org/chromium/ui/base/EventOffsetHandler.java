// Copyright 2018 The Chromium Authors
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
    /** A delegate for EventOffsetHandler. */
    public interface EventOffsetHandlerDelegate {
        float getTop();

        void setCurrentTouchEventOffsets(float top);

        void setCurrentDragEventOffsets(float dx, float dy);
    }

    private final EventOffsetHandlerDelegate mDelegate;

    public EventOffsetHandler(EventOffsetHandlerDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Call this before handling onDispatchDragEvent.
     * @param action Drag event action.
     * @param dx The offset on x-axis for the current drag event.
     * @param dy The offset on y-axis for the current drag event.
     */
    public void onPreDispatchDragEvent(int action, float dx, float dy) {
        setTouchEventOffsets(-mDelegate.getTop());
        setDragEventOffsets(dx, dy);
    }

    /**
     * Call this after handling onDispatchDragEvent.
     * @param action Drag event action.
     */
    public void onPostDispatchDragEvent(int action) {
        if (action == DragEvent.ACTION_DRAG_EXITED
                || action == DragEvent.ACTION_DRAG_ENDED
                || action == DragEvent.ACTION_DROP) {
            setTouchEventOffsets(0.f);
            setDragEventOffsets(0.f, 0.f);
        }
    }

    /**
     * EventOffsetHandler will see only touch downs, and rest of the touch sequence is not
     * guaranteed to be seen post InputVizard[1] where the input sequence will be transferred to viz
     * and the parent view does not gets a change to intercept those events. [1]
     * https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE
     */
    public void onInterceptTouchDownEvent(MotionEvent e) {
        int actionMasked = SPenSupport.convertSPenEventAction(e.getActionMasked());
        assert (actionMasked == MotionEvent.ACTION_DOWN);
        setContentViewMotionEventOffsets(e, /* canClear= */ false);
    }

    /** See {@link View#onTouchEvent(MotionEvent)}. */
    public void onTouchEvent(MotionEvent e) {
        setContentViewMotionEventOffsets(e, /* canClear= */ true);
    }

    /** See {@link ViewGroup#onInterceptHoverEvent(MotionEvent)}. */
    public void onInterceptHoverEvent(MotionEvent e) {
        setContentViewMotionEventOffsets(e, /* canClear= */ true);
    }

    /** See {@link ViewGroup#onHoverEvent(MotionEvent)}. */
    public void onHoverEvent(MotionEvent e) {
        setContentViewMotionEventOffsets(e, /* canClear= */ true);
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

    private void setDragEventOffsets(float dx, float dy) {
        mDelegate.setCurrentDragEventOffsets(dx, dy);
    }
}

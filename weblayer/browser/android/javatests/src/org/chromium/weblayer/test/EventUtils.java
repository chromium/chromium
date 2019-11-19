// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

/**
 * Utilities related to event generation.
 */
public final class EventUtils {
    private EventUtils() {}

    /**
     * Asynchronously posts a touch-down and touch-up event at the center of the supplied View.
     */
    public static void simulateTouchCenterOfView(final View view) {
        simulateDragFromCenterOfView(view, 0, 0);
    }

    /**
     * Asynchronously posts a touch-down and touch-up event at the center of the supplied View. If
     * deltaX or deltaY is non-zero a touch-move is generated between the down/up.
     */
    public static void simulateDragFromCenterOfView(
            final View view, final float deltaX, final float deltaY) {
        view.post(() -> {
            long eventTime = SystemClock.uptimeMillis();
            float x = (float) (view.getRight() - view.getLeft()) / 2;
            float y = (float) (view.getBottom() - view.getTop()) / 2;
            view.dispatchTouchEvent(
                    MotionEvent.obtain(eventTime, eventTime, MotionEvent.ACTION_DOWN, x, y, 0));
            if (deltaX != 0 || deltaY != 0) {
                view.dispatchTouchEvent(MotionEvent.obtain(
                        eventTime, eventTime, MotionEvent.ACTION_MOVE, x + deltaX, y + deltaY, 0));
            }
            view.dispatchTouchEvent(MotionEvent.obtain(
                    eventTime, eventTime, MotionEvent.ACTION_UP, x + deltaX, y + deltaY, 0));
        });
    }
}

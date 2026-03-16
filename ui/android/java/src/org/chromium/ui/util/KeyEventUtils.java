// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.view.KeyEvent;

import org.chromium.build.annotations.NullMarked;

/** Class with helper methods for {@link KeyEvent}. */
@NullMarked
public class KeyEventUtils {
    /**
     * Returns whether the control key is down.
     *
     * @param metaState The meta state from a {@link KeyEvent} or {@link android.view.MotionEvent}.
     * @return Whether the control key is down.
     */
    public static boolean isCtrlOn(int metaState) {
        return (metaState & KeyEvent.META_CTRL_ON) != 0;
    }

    /**
     * Returns whether the shift key is down.
     *
     * @param metaState The meta state from a {@link KeyEvent} or {@link android.view.MotionEvent}.
     * @return Whether the shift key is down.
     */
    public static boolean isShiftOn(int metaState) {
        return (metaState & KeyEvent.META_SHIFT_ON) != 0;
    }

    /**
     * Returns whether the alt key is down.
     *
     * @param metaState The meta state from a {@link KeyEvent} or {@link android.view.MotionEvent}.
     * @return Whether the alt key is down.
     */
    public static boolean isAltOn(int metaState) {
        return (metaState & KeyEvent.META_ALT_ON) != 0;
    }
}

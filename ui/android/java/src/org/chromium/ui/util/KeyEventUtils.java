// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.view.InputDevice;
import android.view.KeyEvent;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Class with helper methods for {@link KeyEvent}. */
@NullMarked
public class KeyEventUtils {
    private static @Nullable Boolean sIsFromAlphabeticKeyboardForTesting;

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

    /** Modifies the output of {@link #isFromAlphabeticKeyboard(KeyEvent)} for testing. */
    public static void setIsFromAlphabeticKeyboardForTesting(Boolean isFromAlphabeticKeyboard) {
        sIsFromAlphabeticKeyboardForTesting = isFromAlphabeticKeyboard;
        ResettersForTesting.register(() -> sIsFromAlphabeticKeyboardForTesting = null);
    }

    /** Returns whether the given {@link KeyEvent} originated from an alphabetic keyboard. */
    public static boolean isFromAlphabeticKeyboard(KeyEvent event) {
        if (sIsFromAlphabeticKeyboardForTesting != null) {
            return sIsFromAlphabeticKeyboardForTesting;
        }
        InputDevice device = event.getDevice();
        return device != null && device.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC;
    }
}

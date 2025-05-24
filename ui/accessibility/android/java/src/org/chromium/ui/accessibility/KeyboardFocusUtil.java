// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;

@NullMarked
public class KeyboardFocusUtil {
    /**
     * Returns whether we successfully set focus on {@param viewGroup}'s 1st focusable descendant.
     *
     * @param viewGroup The {@link ViewGroup} whose 1st focusable descendant we want to focus.
     * @return Whether we successfully set focus on any item.
     */
    public static boolean setFocusOnFirstFocusableDescendant(ViewGroup viewGroup) {
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View childView = viewGroup.getChildAt(i);
            if (childView instanceof ViewGroup focusableGroup) {
                if (setFocusOnFirstFocusableDescendant(focusableGroup)) return true;
            }
            if (!childView.isFocusable()) {
                continue;
            }
            if (setFocus(childView)) return true;
        }
        return setFocus(viewGroup);
    }

    /**
     * Sets focus on {@param view}, regardless of whether {@param view} is focusable in touch mode.
     *
     * @param view The view to set keyboard focus on.
     * @return Whether we successfully set focus on {@param view}.
     */
    public static boolean setFocus(View view) {
        if (!view.isFocusable()) return false;
        boolean wasFocusableInTouchMode = view.isFocusableInTouchMode();
        view.setFocusableInTouchMode(true);
        // requestFocus fails if the view is not visible. In that case, we want to return false to
        // setFocusOnFirstFocusableDescendant so that the focus will go through.
        boolean result = view.requestFocus();
        view.setFocusableInTouchMode(wasFocusableInTouchMode);
        return result;
    }
}

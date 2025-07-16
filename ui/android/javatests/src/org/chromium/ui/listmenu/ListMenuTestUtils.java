// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Test utilities for ListMenu. */
public final class ListMenuTestUtils {

    /**
     * Set the callback to capture the current popup window when its shown / dismissed and return
     * the Mockito spy. This is useful for features that wants to verify the menu properties within
     * the menu.
     *
     * Example usage:
     * <pre>
     * {@code
     * public void myTest() {
     *     captureListMenuHostSpy(menu -> mSpyPopupMenu = menu);
     *     // Call some method that shows the menu
     *     assertNotNull(mSpyPopupMenu);
     *
     *     verify(mSpyListMenu).setMaxWidth(anyInt());
     * }
     */
    public static void captorPopupWindowSpy(Callback<AnchoredPopupWindow> popupWindowCallback) {
        ListMenuHost.setMenuChangedListenerForTesting(
                menu -> {
                    if (menu != null) menu = Mockito.spy(menu);
                    popupWindowCallback.onResult(menu);
                    return menu;
                });
    }
}

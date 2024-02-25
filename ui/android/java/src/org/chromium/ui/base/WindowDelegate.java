// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.graphics.Rect;
import android.view.Window;

/** This is a delegate that handles communication about a window's current state and properties. */
public class WindowDelegate {
    private final Window mWindow;

    /**
     * @param window The Window object that this delegate will communicate with.
     */
    public WindowDelegate(Window window) {
        mWindow = window;
    }

    /**
     * @return The soft input mode that is currently used by the window.
     */
    public int getWindowSoftInputMode() {
        return mWindow.getAttributes().softInputMode;
    }

    /**
     * Set the soft input mode that is used by the window.
     * @param softInputMode The soft input mode to be used.
     */
    public void setWindowSoftInputMode(int softInputMode) {
        mWindow.setSoftInputMode(softInputMode);
    }

    /**
     * Used for accessing the current display frame for the whole window view hierarchy.
     * @param displayFrame The rect that will be set to the current display frame for the window.
     */
    public void getWindowVisibleDisplayFrame(Rect displayFrame) {
        mWindow.getDecorView().getWindowVisibleDisplayFrame(displayFrame);
    }

    /**
     * @return The height of the decor view.
     */
    public int getDecorViewHeight() {
        return mWindow.getDecorView().getHeight();
    }
}

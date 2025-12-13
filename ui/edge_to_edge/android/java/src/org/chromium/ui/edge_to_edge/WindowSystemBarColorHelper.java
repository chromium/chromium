// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import android.app.Activity;
import android.view.Window;

import org.chromium.build.annotations.NullMarked;

/** A wrapper class around {@link Window} to change the system bar colors. */
@NullMarked
public class WindowSystemBarColorHelper extends BaseSystemBarColorHelper {
    private final Window mWindow;

    /**
     * @param window Window in from {@link Activity#getWindow()}.
     */
    public WindowSystemBarColorHelper(Window window) {
        mWindow = window;

        mStatusBarColor = mWindow.getStatusBarColor();
        mNavBarColor = mWindow.getNavigationBarColor();
        mNavBarDividerColor = mWindow.getNavigationBarDividerColor();
    }

    @Override
    public int getStatusBarColor() {
        return mWindow.getStatusBarColor();
    }

    /** Wrapper call to {@link Window#setStatusBarColor(int)} (int)}. */
    @Override
    protected void applyStatusBarColor() {
        mWindow.setStatusBarColor(mStatusBarColor);
    }

    @Override
    public int getNavigationBarColor() {
        return mWindow.getNavigationBarColor();
    }

    /** Wrapper call to {@link Window#setNavigationBarColor(int)}. */
    @Override
    public void applyNavBarColor() {
        mWindow.setNavigationBarColor(mNavBarColor);
    }

    /** Wrapper call to {@link Window#setNavigationBarDividerColor(int)}} */
    @Override
    protected void applyNavigationBarDividerColor() {
        mWindow.setNavigationBarDividerColor(mNavBarDividerColor);
    }

    @Override
    public int getNavigationBarDividerColor() {
        return mWindow.getNavigationBarDividerColor();
    }

    @Override
    public void destroy() {}

    /** Wrapper call to {@link Window#setNavigationBarContrastEnforced(boolean)}. */
    public void setNavigationBarContrastEnforced(boolean enforced) {
        mWindow.setNavigationBarContrastEnforced(enforced);
    }

    /** Wrapper call to {@link Window#setStatusBarContrastEnforced(boolean)}. */
    public void setStatusBarContrastEnforced(boolean enforced) {
        mWindow.setStatusBarContrastEnforced(enforced);
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import android.graphics.Color;
import android.view.View;
import android.view.Window;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.UiUtils;
import org.chromium.ui.util.ColorUtils;

/** Base implementation of a {@link SystemBarColorHelper} that tracks colors internally. */
@NullMarked
public abstract class BaseSystemBarColorHelper implements SystemBarColorHelper {
    protected @ColorInt int mStatusBarColor = Color.TRANSPARENT;
    protected @ColorInt int mNavBarColor = Color.TRANSPARENT;
    protected @ColorInt int mNavBarDividerColor = Color.TRANSPARENT;

    @Override
    public void setStatusBarColor(@ColorInt int color) {
        if (color == getStatusBarColor()) return;
        mStatusBarColor = color;
        applyStatusBarColor();
    }

    @Override
    public void setNavigationBarColor(@ColorInt int color) {
        if (color == getNavigationBarColor()) return;
        mNavBarColor = color;
        applyNavBarColor();
    }

    /** Wrapper call to {@link Window#setNavigationBarDividerColor(int)}} */
    @Override
    public void setNavigationBarDividerColor(@ColorInt int dividerColor) {
        if (dividerColor == getNavigationBarDividerColor()) return;
        mNavBarDividerColor = dividerColor;
        applyNavigationBarDividerColor();
    }

    /** Return the current status bar color tracked by this instance. */
    public @ColorInt int getStatusBarColor() {
        return mStatusBarColor;
    }

    /** Return the current nav bar color tracked by this instance. */
    public @ColorInt int getNavigationBarColor() {
        return mNavBarColor;
    }

    /**
     * Sets the status bar icons to dark or light based on the luminance of mStatusBarColor to
     * ensure enough contrast.
     */
    protected static void updateStatusBarIconColor(View rootView, @ColorInt int statusBarColor) {
        UiUtils.setStatusBarIconColor(
                rootView,
                ColorUtils.isHighLuminance(ColorUtils.calculateLuminance(statusBarColor)));
    }

    /**
     * Sets the navigation bar icons to dark or light based on the luminance of mNavBarColor to
     * ensure enough contrast.
     */
    protected static void updateNavigationBarIconColor(
            View rootView, @ColorInt int navigationBarColor) {
        UiUtils.setNavigationBarIconColor(
                rootView,
                ColorUtils.isHighLuminance(ColorUtils.calculateLuminance(navigationBarColor)));
    }

    /** Return the current nav bar divider color tracked by this instance. */
    public @ColorInt int getNavigationBarDividerColor() {
        return mNavBarDividerColor;
    }

    protected abstract void applyStatusBarColor();

    protected abstract void applyNavBarColor();

    protected abstract void applyNavigationBarDividerColor();
}

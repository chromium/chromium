// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import android.graphics.Color;
import android.view.Window;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Helper class that coordinates whether to apply the color changes to system window, or external
 * delegate based on the edge to edge status for the current activity window. When the window switch
 * to drawing edge to edge, the window's nav bar and status bar will be set to Color.TRANSPARENT.
 *
 * <p>This instance is meant to be created at the based activity level, and one instance per
 * activity. This class will use the window's bar color when it's initialized.
 */
@NullMarked
public class EdgeToEdgeSystemBarColorHelper extends BaseSystemBarColorHelper {
    private final ObservableSupplier<Boolean> mDoesContentFitWindowSupplier;
    private final OneshotSupplier<SystemBarColorHelper> mEdgeToEdgeDelegateHelperSupplier;
    private final Window mWindow;
    private WindowSystemBarColorHelper mWindowColorHelper;
    private final Callback<Boolean> mOnEdgeToEdgeChanged = this::onContentFitsWindowChanged;

    protected boolean mIsActivityEdgeToEdge;
    protected boolean mCanColorStatusBarColor;

    /**
     * @param window Window from {@link android.app.Activity#getWindow()}.
     * @param doesContentFitWindowSupplier Supplier of whether the activity content fits the window
     *     insets.
     * @param delegateHelperSupplier Delegate helper that colors the bar when edge to edge.
     * @param canColorStatusBarColor Value of the EdgeToEdgeEverywhere flag. Determines whether the
     *     status bar color could be colored.
     */
    public EdgeToEdgeSystemBarColorHelper(
            Window window,
            ObservableSupplier<Boolean> doesContentFitWindowSupplier,
            OneshotSupplier<SystemBarColorHelper> delegateHelperSupplier,
            boolean canColorStatusBarColor) {
        mWindow = window;
        mDoesContentFitWindowSupplier = doesContentFitWindowSupplier;
        mEdgeToEdgeDelegateHelperSupplier = delegateHelperSupplier;
        mWindowColorHelper = new WindowSystemBarColorHelper(window);
        mCanColorStatusBarColor = canColorStatusBarColor;

        // Initial values. By default, read the values from window.
        mIsActivityEdgeToEdge = Boolean.FALSE.equals(mDoesContentFitWindowSupplier.get());
        mStatusBarColor = mWindowColorHelper.getStatusBarColor();
        mNavBarColor = mWindowColorHelper.getNavigationBarColor();
        mNavBarDividerColor = mWindowColorHelper.getNavigationBarDividerColor();

        mDoesContentFitWindowSupplier.addObserver(mOnEdgeToEdgeChanged);
        mEdgeToEdgeDelegateHelperSupplier.onAvailable(this::onDelegateColorHelperChanged);
    }

    @Override
    public void destroy() {
        mDoesContentFitWindowSupplier.removeObserver(mOnEdgeToEdgeChanged);
        mWindowColorHelper.destroy();
    }

    @Override
    protected void applyStatusBarColor() {
        updateStatusBarColor();
    }

    @Override
    protected void applyNavBarColor() {
        updateNavBarColors();
    }

    @Override
    protected void applyNavigationBarDividerColor() {
        updateNavBarColors();
    }

    private void onContentFitsWindowChanged(Boolean contentFitsWindow) {
        boolean toEdge = Boolean.FALSE.equals(contentFitsWindow);
        if (mIsActivityEdgeToEdge != toEdge) {
            mIsActivityEdgeToEdge = toEdge;
            updateNavBarColors();
            updateStatusBarColor();
        }
    }

    private void onDelegateColorHelperChanged(SystemBarColorHelper delegate) {
        updateStatusBarColor();
        updateNavBarColors();
    }

    private void updateNavBarColors() {
        int windowNavColor = mIsActivityEdgeToEdge ? Color.TRANSPARENT : mNavBarColor;
        int windowNavDividerColor = mIsActivityEdgeToEdge ? Color.TRANSPARENT : mNavBarDividerColor;
        mWindowColorHelper.setNavigationBarColor(windowNavColor);
        mWindowColorHelper.setNavigationBarDividerColor(windowNavDividerColor);
        // When setting a transparent navbar for drawing toEdge, the system navbar contrast
        // should not be enforced - otherwise, some devices will apply a scrim to the navbar.
        mWindowColorHelper.setNavigationBarContrastEnforced(!mIsActivityEdgeToEdge);

        SystemBarColorHelper delegateHelper = mEdgeToEdgeDelegateHelperSupplier.get();
        if (delegateHelper != null && mIsActivityEdgeToEdge) {
            delegateHelper.setNavigationBarColor(mNavBarColor);
            delegateHelper.setNavigationBarDividerColor(mNavBarDividerColor);
        }

        updateNavigationBarIconColor(mWindow.getDecorView(), mNavBarColor);
    }

    private void updateStatusBarColor() {
        if (!canSetStatusBarColor()) {
            return;
        }
        SystemBarColorHelper delegateHelper = mEdgeToEdgeDelegateHelperSupplier.get();
        // In ChromeTabbedActivity the delegate is null because native has not initialized. Prevents
        // setting the window status bar to transparent when the delegate is null.
        int windowStatusBarColor = mStatusBarColor;
        if (mIsActivityEdgeToEdge
                && delegateHelper != null
                && delegateHelper.canSetStatusBarColor()) {
            delegateHelper.setStatusBarColor(mStatusBarColor);
            windowStatusBarColor = Color.TRANSPARENT;
        }

        mWindowColorHelper.setStatusBarColor(windowStatusBarColor);
        mWindowColorHelper.setStatusBarContrastEnforced(!mIsActivityEdgeToEdge);

        updateStatusBarIconColor(mWindow.getDecorView(), mStatusBarColor);
    }

    public WindowSystemBarColorHelper getWindowHelperForTesting() {
        return mWindowColorHelper;
    }

    public void setWindowHelperForTesting(WindowSystemBarColorHelper helper) {
        mWindowColorHelper = helper;
    }

    @Nullable SystemBarColorHelper getEdgeToEdgeDelegateHelperForTesting() {
        return mEdgeToEdgeDelegateHelperSupplier.get();
    }

    @Override
    public boolean canSetStatusBarColor() {
        return mCanColorStatusBarColor;
    }
}

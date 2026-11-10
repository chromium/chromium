// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.WindowManager;
import android.widget.PopupWindow;

import org.chromium.build.annotations.NullMarked;

/**
 * A subclass of {@link PopupWindow} that allows custom layout configurations, such as allowing the
 * popup to overlap the caption bar / window decorations in desktop mode.
 */
@NullMarked
public class ChromePopupWindow extends PopupWindow {
    private boolean mAllowOverlapCaptionBar;

    public ChromePopupWindow(Context context) {
        super(context);
    }

    /**
     * Sets whether this popup window is allowed to overlap the caption bar/window decorations in
     * desktop mode.
     */
    public void setAllowOverlapCaptionBar(boolean allow) {
        mAllowOverlapCaptionBar = allow;
    }

    @Override
    public void showAtLocation(View parent, int gravity, int x, int y) {
        super.showAtLocation(parent, gravity, x, y);
        if (mAllowOverlapCaptionBar && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            View contentView = getContentView();
            if (contentView != null) {
                ViewParent decorView = contentView.getParent();
                if (decorView instanceof View view) {
                    ViewGroup.LayoutParams lp = view.getLayoutParams();
                    if (lp instanceof WindowManager.LayoutParams wp) {
                        // Passing 0 (empty bitmask) tells the WindowManager not to fit any system
                        // insets for this window, allowing it to overlap the caption bar area.
                        wp.setFitInsetsTypes(0);
                        WindowManager wm =
                                (WindowManager)
                                        parent.getContext()
                                                .getSystemService(Context.WINDOW_SERVICE);
                        if (wm != null) {
                            wm.updateViewLayout((View) decorView, wp);
                        }
                    }
                }
            }
        }
    }
}

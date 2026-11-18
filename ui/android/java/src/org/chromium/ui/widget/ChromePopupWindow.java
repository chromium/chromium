// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.WindowManager;
import android.widget.PopupWindow;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.UiAndroidFeatureList;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 * A subclass of {@link PopupWindow} that allows custom layout configurations, such as allowing the
 * popup to overlap the caption bar / window decorations in desktop mode, and forwards unhandled
 * browser keyboard shortcuts to the host {@link Activity}.
 */
@NullMarked
public class ChromePopupWindow extends PopupWindow {
    private static final List<WeakReference<ChromePopupWindow>> sShowingPopups = new ArrayList<>();

    private final Context mContext;
    private final View.OnUnhandledKeyEventListener mUnhandledKeyListener =
            (_, event) -> handleUnhandledKeyEvent(event);
    private boolean mAllowOverlapCaptionBar;

    public ChromePopupWindow(Context context) {
        super(context);
        mContext = context;
    }

    /**
     * Sets whether this popup window is allowed to overlap the caption bar/window decorations in
     * desktop mode.
     */
    public void setAllowOverlapCaptionBar(boolean allow) {
        mAllowOverlapCaptionBar = allow;
    }

    @Override
    public void setContentView(@Nullable View contentView) {
        View previousContentView = getContentView();
        super.setContentView(contentView);
        if (!UiAndroidFeatureList.sAndroidUnifiedKeyboardShortcuts.isEnabled()
                || previousContentView == contentView) {
            return;
        }
        if (previousContentView != null) {
            previousContentView.removeOnUnhandledKeyEventListener(mUnhandledKeyListener);
        }
        if (contentView != null) {
            contentView.addOnUnhandledKeyEventListener(mUnhandledKeyListener);
        }
    }

    @Override
    public void showAtLocation(View parent, int gravity, int x, int y) {
        super.showAtLocation(parent, gravity, x, y);
        if (UiAndroidFeatureList.sAndroidUnifiedKeyboardShortcuts.isEnabled() && isShowing()) {
            sShowingPopups.removeIf(ref -> ref.get() == null || ref.get() == this);
            sShowingPopups.add(new WeakReference<>(this));
        }
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

    @Override
    public void showAsDropDown(View anchor, int xoff, int yoff, int gravity) {
        super.showAsDropDown(anchor, xoff, yoff, gravity);
        if (UiAndroidFeatureList.sAndroidUnifiedKeyboardShortcuts.isEnabled() && isShowing()) {
            sShowingPopups.removeIf(ref -> ref.get() == null || ref.get() == this);
            sShowingPopups.add(new WeakReference<>(this));
        }
    }

    @Override
    public void dismiss() {
        if (UiAndroidFeatureList.sAndroidUnifiedKeyboardShortcuts.isEnabled()) {
            sShowingPopups.removeIf(ref -> ref.get() == null || ref.get() == this);
        }
        super.dismiss();
    }

    private boolean handleUnhandledKeyEvent(KeyEvent event) {
        if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() != 0) {
            return false;
        }
        Activity activity = ContextUtils.activityFromContext(mContext);
        if (activity == null) return false;
        // Snapshot open popups before onKeyDown so we don't dismiss a new popup opened by the
        // shortcut itself (e.g. Alt+E opening AppMenu) or hit ConcurrentModificationException.
        List<ChromePopupWindow> popupsToDismiss = new ArrayList<>();
        for (WeakReference<ChromePopupWindow> ref : sShowingPopups) {
            ChromePopupWindow popup = ref.get();
            if (popup != null && ContextUtils.activityFromContext(popup.mContext) == activity) {
                popupsToDismiss.add(popup);
            }
        }
        if (activity.onKeyDown(event.getKeyCode(), event)) {
            for (ChromePopupWindow popup : popupsToDismiss) {
                popup.dismiss();
            }
            return true;
        }
        return false;
    }
}

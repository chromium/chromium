// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.PopupWindow;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.BuildConfig;
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
    // TODO(crbug.com/542609750): Unify sShowingPopupsForTesting with sShowingPopups once
    // ANDROID_UNIFIED_KEYBOARD_SHORTCUTS launches and the flag is cleaned up.
    private static final @Nullable List<WeakReference<ChromePopupWindow>> sShowingPopupsForTesting =
            BuildConfig.IS_FOR_TEST ? new ArrayList<>() : null;

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
        trackShowingPopup();
        maybeAllowOverlapCaptionBar(parent.getContext());
    }

    @Override
    public void showAsDropDown(View anchor, int xoff, int yoff, int gravity) {
        super.showAsDropDown(anchor, xoff, yoff, gravity);
        trackShowingPopup();
        maybeAllowOverlapCaptionBar(anchor.getContext());
    }

    @Override
    public void dismiss() {
        if (UiAndroidFeatureList.sAndroidUnifiedKeyboardShortcuts.isEnabled()) {
            sShowingPopups.removeIf(ref -> ref.get() == null || ref.get() == this);
        }
        if (sShowingPopupsForTesting != null) {
            sShowingPopupsForTesting.removeIf(ref -> ref.get() == null || ref.get() == this);
        }
        super.dismiss();
    }

    /**
     * Dismisses all currently showing {@link ChromePopupWindow} instances.
     *
     * @return The number of showing popups dismissed.
     */
    public static int dismissAllPopupsForTesting() {
        ThreadUtils.assertOnUiThread();
        if (sShowingPopupsForTesting == null) return 0;
        List<ChromePopupWindow> popupsToDismiss = new ArrayList<>();
        for (WeakReference<ChromePopupWindow> ref : sShowingPopupsForTesting) {
            ChromePopupWindow popup = ref.get();
            if (popup != null && popup.isShowing()) {
                popupsToDismiss.add(popup);
            }
        }
        int dismissedCount = 0;
        for (ChromePopupWindow popup : popupsToDismiss) {
            if (popup.isShowing()) {
                popup.dismiss();
                dismissedCount++;
            }
        }
        sShowingPopupsForTesting.removeIf(
                ref -> {
                    ChromePopupWindow popup = ref.get();
                    return popup == null || !popup.isShowing();
                });
        return dismissedCount;
    }

    private void trackShowingPopup() {
        if (!isShowing()) return;
        if (UiAndroidFeatureList.sAndroidUnifiedKeyboardShortcuts.isEnabled()) {
            sShowingPopups.removeIf(ref -> ref.get() == null || ref.get() == this);
            sShowingPopups.add(new WeakReference<>(this));
        }
        if (sShowingPopupsForTesting != null) {
            sShowingPopupsForTesting.removeIf(ref -> ref.get() == null || ref.get() == this);
            sShowingPopupsForTesting.add(new WeakReference<>(this));
        }
    }

    private void maybeAllowOverlapCaptionBar(Context context) {
        if (!mAllowOverlapCaptionBar
                || Build.VERSION.SDK_INT < Build.VERSION_CODES.R
                || !isShowing()) {
            return;
        }
        View contentView = getContentView();
        if (contentView == null) return;
        // PopupWindow wraps contentView in PopupBackgroundView when a background drawable is set,
        // so walk to the root PopupDecorView that holds WindowManager.LayoutParams.
        View decorView = contentView.getRootView();
        if (decorView.getParent() != null
                && decorView.getLayoutParams() instanceof WindowManager.LayoutParams wp) {
            // Clear only captionBar() so the popup can overlap the desktop caption bar while still
            // clamping against status bars, navigation bars / taskbar, and system overlays.
            wp.setFitInsetsTypes(wp.getFitInsetsTypes() & ~WindowInsets.Type.captionBar());
            WindowManager wm = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
            if (wm != null) {
                wm.updateViewLayout(decorView, wp);
            }
        }
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

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;

/**
 * Informed of interesting events that happen during the lifetime of a Tab.
 */
public abstract class TabCallback {
    /**
     * The Uri that should be displayed in the location-bar has updated.
     *
     * @param uri The new user-visible uri.
     */
    public void onVisibleUriChanged(@NonNull Uri uri) {}

    /**
     * Triggered when the render process dies, either due to crash or killed by the system to
     * reclaim memory.
     */
    public void onRenderProcessGone() {}

    /**
     * Triggered when a context menu should be displayed.
     */
    public void showContextMenu(@NonNull ContextMenuParams params) {}

    /**
     * Triggered when a tab's contents have been rendered inactive due to a modal overlay, or active
     * due to the dismissal of a modal overlay (dialog/bubble/popup).
     *
     * @param isTabModalShowing true when a dialog is blocking interaction with the web contents.
     */
    public void onTabModalStateChanged(boolean isTabModalShowing) {}

    /**
     * Called when the title of this tab changes. Note before the page sets a title, the title may
     * be a portion of the Uri.
     * @param title New title of this tab.
     */
    public void onTitleUpdated(@NonNull String title) {}

    /**
     * Called when user attention should be brought to this tab. This should cause the tab, its
     * containing Activity, and the task to be foregrounded.
     */
    public void bringTabToFront() {}

    /**
     * Called when then background color of the page changes. The background color typically comes
     * from css background-color, but heuristics and blending may be used depending upon the page.
     * This is mostly useful for filling in gaps around the web page during resize, but it will
     * not necessarily match the full background of the page.
     * @param color The new ARGB color of the page background.
     */
    public void onBackgroundColorChanged(int color) {}

    /**
     * Notification for scroll of the root of the web page. This is generally sent as a result of
     * displaying web page. See ScrollNotificationType for more details. ScrollNotificationType is
     * meant to be extensible and new types may be added in the future. Embedder should take care
     * to allow unknown values.
     * @param notificationType type of notification. See ScrollNotificationType for more details.
     * @param currentScrollRatio value in [0, 1] indicating the current scroll ratio. For example
     *                           a web page that is 200 pixels, has a viewport of height 50 pixels
     *                           and a scroll offset of 50 pixels will have a ratio of 0.5.
     */
    public void onScrollNotification(
            @ScrollNotificationType int notificationType, float currentScrollRatio) {}
}

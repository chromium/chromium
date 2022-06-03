// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Callback notified when the vertical location of the top or bottom View changes.
 *
 * WebLayer maintains a snapshot (bitmap) of the browser controls. During a scroll, the snapshot is
 * scrolled and not the actual View. A ramification of this is any updates to the View during a
 * scroll are not reflected to the user. If the contents of the controls needs to change during a
 * scroll, than an empty View should be set as the top/bottom control and the real View should be
 * positioned based on the offsets supplied to the callback.
 *
 * @since 88
 */
public abstract class BrowserControlsOffsetCallback {
    /**
     * Called when the vertical location of the top view changes. The value varies from 0
     * (completely shown) to -(height - minHeight), where height is the preferred height of the view
     * and minHeight is the minimum height supplied to {@link Tab#setTopView}.
     *
     * If the top view is removed, this is called with a value of 0.
     *
     * @param offset The vertical offset.
     */
    public void onTopViewOffsetChanged(int offset) {}

    /**
     * Called when the vertical location of the bottom view changes. The value varies from 0
     * (completely shown) to the preferred height of the bottom view.
     *
     * If the bottom view is removed, this is called with a value of 0.
     *
     * @param offset The vertical offset.
     */
    public void onBottomViewOffsetChanged(int offset) {}
}

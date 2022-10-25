// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;

/**
 * An interface for observing changes to a Tab.
 */
public abstract class TabObserver {
    /**
     * The Uri that can be displayed in the location-bar has updated.
     *
     * @param uri The new user-visible uri.
     */
    public void onVisibleUriChanged(@NonNull String uri) {}

    /**
     * Called when the title of this tab changes. Note before the page sets a title, the title may
     * be a portion of the Uri.
     *
     * @param title New title of this tab.
     */
    public void onTitleUpdated(@NonNull String title) {}

    /**
     * Triggered when the render process dies, either due to crash or killed by the system to
     * reclaim memory.
     */
    public void onRenderProcessGone() {}
}
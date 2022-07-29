// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An interface for observing changes to the set of tabs in a BrowserFragment.
 */
public abstract class TabObserver {
    /**
     * The active tab has changed.
     *
     * @param activeTab The newly active tab, null if no tab is active.
     */
    public void onActiveTabChanged(@Nullable Tab activeTab) {}

    /**
     * A tab was added to the BrowserFragment.
     *
     * @param tab The tab that was added.
     */
    public void onTabAdded(@NonNull Tab tab) {}

    /**
     * A tab was removed from the BrowserFragment.
     *
     * WARNING: this is *not* called when the  BrowserFragment is destroyed. See {@link
     * #onWillDestroyBrowserAndAllTabs} for more.
     *
     * @param tab The tab that was removed.
     */
    public void onTabRemoved(@NonNull Tab tab) {}

    /**
     * Called when the BrowserFragment is about to be destroyed. After this
     * call the BrowserFragment with all Tabs are destroyed and can not be used.
     */
    public void onWillDestroyBrowserAndAllTabs() {}
}
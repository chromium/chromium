// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An interface for observing changes to the set of tabs in a browser.
 */
abstract class TabListCallback {
    /**
     * The active tab has changed.
     *
     * @param activeTab The newly active tab, null if no tab is active.
     */
    public void onActiveTabChanged(@Nullable Tab activeTab) {}

    /**
     * A tab was added to the Browser.
     *
     * @param tab The tab that was added.
     */
    public void onTabAdded(@NonNull Tab tab) {}

    /**
     * A tab was removed from the Browser.
     *
     * WARNING: this is *not* called when the Browser is destroyed. See {@link
     * #onWillDestroyBrowserAndAllTabs} for more.
     *
     * @param tab The tab that was removed.
     */
    public void onTabRemoved(@NonNull Tab tab) {}

    /**
     * Called when the Fragment the Browser is associated with is about to be destroyed. After this
     * call the Browser and all Tabs are destroyed and can not be used.
     */
    public void onWillDestroyBrowserAndAllTabs() {}
}

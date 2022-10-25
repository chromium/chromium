// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An interface for observing changes to the set of Tabs in a WebFragment.
 */
public abstract class TabListObserver {
    /**
     * The active tab has changed.
     *
     * @param activeTab The newly active tab, null if no tab is active.
     */
    public void onActiveTabChanged(@Nullable Tab activeTab) {}

    /**
     * A tab was added to the WebFragment.
     *
     * @param tab The tab that was added.
     */
    public void onTabAdded(@NonNull Tab tab) {}

    /**
     * A tab was removed from the WebFragment.
     *
     * WARNING: this is *not* called when the  WebFragment is destroyed.
     *
     * @param tab The tab that was removed.
     */
    public void onTabRemoved(@NonNull Tab tab) {}

    /**
     * Called when the WebFragment is about to be destroyed. After this
     * call the WebFragment with all Tabs are destroyed and can not be used.
     */
    public void onWillDestroyFragmentAndAllTabs() {}
}
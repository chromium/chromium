// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An interface for observing changes to the set of Tabs in a WebFragment.
 */
public interface TabListObserver {
    /**
     * The active tab has changed.
     *
     * @param webEngine the webEngine associated with this event.
     * @param activeTab The newly active tab, null if no tab is active.
     */
    public default void onActiveTabChanged(@NonNull WebEngine webEngine, @Nullable Tab activeTab) {}

    /**
     * A tab was added to the WebFragment.
     *
     * @param webEngine the webEngine associated with this event.
     * @param tab The tab that was added.
     */
    public default void onTabAdded(@NonNull WebEngine webEngine, @NonNull Tab tab) {}

    /**
     * A tab was removed from the WebFragment.
     *
     * WARNING: this is *not* called when the  WebFragment is destroyed.
     *
     * @param webEngine the webEngine associated with this event.
     * @param tab The tab that was removed.
     */
    public default void onTabRemoved(@NonNull WebEngine webEngine, @NonNull Tab tab) {}

    /**
     * Called when the WebFragment is about to be destroyed. After this
     * call the WebFragment with all Tabs are destroyed and can not be used.
     *
     * @param webEngine the webEngine associated with this event.
     */
    public default void onWillDestroyFragmentAndAllTabs(@NonNull WebEngine webEngine) {}
}
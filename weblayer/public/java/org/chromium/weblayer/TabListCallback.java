// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An interface for observing changes to the set of tabs in a browser.
 */
public abstract class TabListCallback {
    /**
     * The active tab has changed.
     *
     * @param activeTab The newly active tab.
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
     * @param tab The tab that was removed.
     */
    public void onTabRemoved(@NonNull Tab tab) {}
}

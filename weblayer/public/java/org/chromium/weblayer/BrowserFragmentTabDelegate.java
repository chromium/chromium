// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * This class acts as a proxy between the Tab events happening in
 * weblayer and the TabManager in browserfragment.
 */
class BrowserFragmentTabDelegate extends TabListCallback {
    private Tab mActiveTab;

    // TODO(swestphal): Expose these tab events to browserfragment.TabManager.

    @Override
    public void onActiveTabChanged(@Nullable Tab activeTab) {
        mActiveTab = activeTab;
    }

    @Override
    public void onTabAdded(@NonNull Tab tab) {}

    @Override
    public void onTabRemoved(@NonNull Tab tab) {}

    @Override
    public void onWillDestroyBrowserAndAllTabs() {}

    @Nullable
    Tab getActiveTab() {
        return mActiveTab;
    }
}
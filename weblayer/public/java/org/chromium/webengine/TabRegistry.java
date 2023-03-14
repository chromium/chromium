// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import androidx.annotation.Nullable;

import org.chromium.webengine.interfaces.ITabParams;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Tab registry for storing open {@link Tab}s per WebEngine on the webengine side.
 *
 * For internal use only.
 */
class TabRegistry {
    private WebEngine mWebEngine;
    private Map<String, Tab> mGuidToTab = new HashMap<String, Tab>();

    @Nullable
    private Tab mActiveTab;

    TabRegistry(WebEngine webEngine) {
        mWebEngine = webEngine;
    }

    Tab getOrCreateTab(ITabParams tabParams) {
        Tab tab = mGuidToTab.get(tabParams.tabGuid);
        if (tab == null) {
            tab = new Tab(mWebEngine, tabParams);
            mGuidToTab.put(tabParams.tabGuid, tab);
        }
        return tab;
    }

    void removeTab(Tab tab) {
        mGuidToTab.remove(tab.getGuid());
    }

    void invalidate() {
        for (Tab tab : mGuidToTab.values()) {
            tab.invalidate();
        }
        mGuidToTab.clear();
    }

    Set<Tab> getTabs() {
        return new HashSet(mGuidToTab.values());
    }

    void setActiveTab(Tab tab) {
        mActiveTab = tab;
    }

    Tab getActiveTab() {
        return mActiveTab;
    }
}
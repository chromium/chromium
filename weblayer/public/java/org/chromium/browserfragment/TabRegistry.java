// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import org.chromium.browserfragment.interfaces.ITabParams;

import java.util.HashMap;
import java.util.Map;

/**
 * Tab registry for storing open {@link Tab}s on the browserfragment side.
 *
 * For internal use only.
 */
class TabRegistry {
    private Map<String, Tab> mGuidToTab = new HashMap<String, Tab>();

    private static TabRegistry sInstance;

    private TabRegistry() {}

    static TabRegistry getInstance() {
        if (sInstance == null) {
            sInstance = new TabRegistry();
        }
        return sInstance;
    }

    Tab getOrCreateTab(ITabParams tabParams) {
        Tab tab = mGuidToTab.get(tabParams.tabGuid);
        if (tab == null) {
            tab = new Tab(tabParams);
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
}
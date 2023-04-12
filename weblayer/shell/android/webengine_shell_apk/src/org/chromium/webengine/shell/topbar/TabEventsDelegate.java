// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell.topbar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.TabObserver;
import org.chromium.webengine.WebEngine;

/**
 * Delegate for Tab Events.
 */
public class TabEventsDelegate implements TabObserver, NavigationObserver, TabListObserver {
    private TabEventsObserver mTabEventsObserver;
    private final TabManager mTabManager;

    public TabEventsDelegate(TabManager tabManager) {
        mTabManager = tabManager;
        mTabManager.registerTabListObserver(this);
        for (Tab t : mTabManager.getAllTabs()) {
            t.getNavigationController().registerNavigationObserver(this);
            t.registerTabObserver(this);
        }
    }

    public void registerObserver(TabEventsObserver tabEventsObserver) {
        mTabEventsObserver = tabEventsObserver;
    }

    public void unregisterObserver() {
        mTabEventsObserver = null;
    }

    // TabObserver implementation.

    @Override
    public void onVisibleUriChanged(@NonNull Tab tab, @NonNull String uri) {
        if (mTabEventsObserver == null) {
            return;
        }
        if (!isTabActive(tab)) {
            return;
        }
        mTabEventsObserver.onVisibleUriChanged(uri);
    }

    @Override
    public void onTitleUpdated(Tab tab, @NonNull String title) {}

    @Override
    public void onRenderProcessGone(Tab tab) {}

    // TabListObserver implementation.

    @Override
    public void onActiveTabChanged(@NonNull WebEngine webEngine, @Nullable Tab activeTab) {
        if (mTabEventsObserver == null) {
            return;
        }
        if (activeTab == null) {
            return;
        }
        mTabEventsObserver.onActiveTabChanged(activeTab);
    }

    @Override
    public void onTabAdded(@NonNull WebEngine webEngine, @NonNull Tab tab) {
        if (mTabEventsObserver == null) {
            return;
        }
        mTabEventsObserver.onTabAdded(tab);

        tab.registerTabObserver(this);
        tab.getNavigationController().registerNavigationObserver(this);
    }

    @Override
    public void onTabRemoved(@NonNull WebEngine webEngine, @NonNull Tab tab) {
        if (mTabEventsObserver == null) {
            return;
        }
        mTabEventsObserver.onTabRemoved(tab);
    }

    @Override
    public void onWillDestroyFragmentAndAllTabs(@NonNull WebEngine webEngine) {}

    // NavigationObserver implementation.

    @Override
    public void onNavigationFailed(@NonNull Tab tab, @NonNull Navigation navigation) {}
    @Override
    public void onNavigationCompleted(@NonNull Tab tab, @NonNull Navigation navigation) {}

    @Override
    public void onNavigationStarted(@NonNull Tab tab, @NonNull Navigation navigation) {}

    @Override
    public void onNavigationRedirected(@NonNull Tab tab, @NonNull Navigation navigation) {}

    @Override
    public void onLoadProgressChanged(@NonNull Tab tab, double progress) {
        if (mTabEventsObserver == null) {
            return;
        }
        if (!isTabActive(tab)) {
            return;
        }
        mTabEventsObserver.onLoadProgressChanged(progress);
    }

    boolean isTabActive(Tab tab) {
        return mTabManager.getActiveTab() != null && mTabManager.getActiveTab().equals(tab);
    }
}

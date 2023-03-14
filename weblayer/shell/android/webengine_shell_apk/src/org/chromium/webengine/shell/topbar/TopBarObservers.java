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
 * Top Bar observers for Test Activities.
 */
public class TopBarObservers implements TabObserver, NavigationObserver, TabListObserver {
    final TopBar mTopBar;
    final TabManager mTabManager;
    public TopBarObservers(TopBar topBar, TabManager tabManager) {
        mTopBar = topBar;
        mTabManager = tabManager;

        mTabManager.registerTabListObserver(this);
        mTabManager.getActiveTab().getNavigationController().registerNavigationObserver(this);
        mTabManager.getActiveTab().registerTabObserver(this);
    }

    // TabObserver implementation.

    @Override
    public void onVisibleUriChanged(@NonNull Tab tab, @NonNull String uri) {
        if (!mTopBar.isTabActive(tab)) {
            return;
        }
        mTopBar.setUrlBar(uri);
    }

    @Override
    public void onTitleUpdated(Tab tab, @NonNull String title) {}

    @Override
    public void onRenderProcessGone(Tab tab) {}

    // TabListObserver implementation.

    @Override
    public void onActiveTabChanged(@NonNull WebEngine webEngine, @Nullable Tab activeTab) {
        if (activeTab == null) {
            return;
        }
        mTopBar.setUrlBar(activeTab.getDisplayUri().toString());
        mTopBar.setTabListSelection(activeTab);
    }

    @Override
    public void onTabAdded(@NonNull WebEngine webEngine, @NonNull Tab tab) {
        mTopBar.addTabToList(tab);
        // Recursively add tab and navigation observers to any new tab.
        tab.registerTabObserver(this);
        tab.getNavigationController().registerNavigationObserver(this);
    }

    @Override
    public void onTabRemoved(@NonNull WebEngine webEngine, @NonNull Tab tab) {
        mTopBar.removeTabFromList(tab);
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
        if (!mTopBar.isTabActive(tab)) {
            return;
        }
        mTopBar.setProgress(progress);
    }
}

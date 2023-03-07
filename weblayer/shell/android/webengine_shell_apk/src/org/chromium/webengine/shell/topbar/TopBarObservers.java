// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell.topbar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.TabObserver;

/**
 * Top Bar observers for Test Activities.
 */
public class TopBarObservers {
    final TopBar mTopBar;
    final TabManager mTabManager;
    public TopBarObservers(TopBar topBar, TabManager tabManager) {
        mTopBar = topBar;
        mTabManager = tabManager;

        mTabManager.registerTabListObserver(new TopBarTabListObserver());
        mTabManager.getActiveTab().getNavigationController().registerNavigationObserver(
                new TopBarNavigationObserver(mTabManager.getActiveTab()));
        mTabManager.getActiveTab().registerTabObserver(
                new TopBarTabObserver(mTabManager.getActiveTab()));
    }
    class TopBarTabObserver extends TabObserver {
        final Tab mTab;
        public TopBarTabObserver(Tab tab) {
            mTab = tab;
        }

        @Override
        public void onVisibleUriChanged(@NonNull String uri) {
            if (!mTopBar.isTabActive(mTab)) {
                return;
            }
            mTopBar.setUrlBar(uri);
        }
    }

    class TopBarNavigationObserver extends NavigationObserver {
        final Tab mTab;
        public TopBarNavigationObserver(Tab tab) {
            mTab = tab;
        }

        @Override
        public void onLoadProgressChanged(double progress) {
            if (!mTopBar.isTabActive(mTab)) {
                return;
            }
            mTopBar.setProgress(progress);
        }
    }

    class TopBarTabListObserver extends TabListObserver {
        @Override
        public void onActiveTabChanged(@Nullable Tab activeTab) {
            if (activeTab == null) {
                return;
            }
            mTopBar.setUrlBar(activeTab.getDisplayUri().toString());
            mTopBar.setTabListSelection(activeTab);
        }

        @Override
        public void onTabAdded(@NonNull Tab tab) {
            mTopBar.addTabToList(tab);
            // Recursively add tab and navigation observers to any new tab.
            tab.registerTabObserver(new TopBarTabObserver(tab));
            tab.getNavigationController().registerNavigationObserver(
                    new TopBarNavigationObserver(tab));
        }

        @Override
        public void onTabRemoved(@NonNull Tab tab) {
            mTopBar.removeTabFromList(tab);
        }
    }
}

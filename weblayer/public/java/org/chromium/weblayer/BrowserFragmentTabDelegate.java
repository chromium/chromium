// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.browserfragment.interfaces.ITabObserverDelegate;

/**
 * This class acts as a proxy between the Tab events happening in
 * weblayer and the TabManager in browserfragment.
 */
class BrowserFragmentTabDelegate extends TabListCallback {
    private Tab mActiveTab;

    private ITabObserverDelegate mTabObserver;

    private final NewTabCallback mNewTabCallback = new NewTabCallback() {
        @Override
        public void onNewTab(@NonNull Tab tab, @NewTabType int type) {}
    };

    void setObserver(ITabObserverDelegate observer) {
        mTabObserver = observer;
    }

    @Override
    public void onActiveTabChanged(@Nullable Tab activeTab) {
        mActiveTab = activeTab;
        maybeRunOnTabObserver(observer -> observer.notifyActiveTabChanged(new TabProxy(activeTab)));
    }

    @Override
    public void onTabAdded(@NonNull Tab tab) {
        // This is a requirement to open new tabs.
        tab.setNewTabCallback(mNewTabCallback);

        maybeRunOnTabObserver(observer -> observer.notifyTabAdded(new TabProxy(tab)));
    }

    @Override
    public void onTabRemoved(@NonNull Tab tab) {
        maybeRunOnTabObserver(observer -> observer.notifyTabRemoved(new TabProxy(tab)));
    }

    @Override
    public void onWillDestroyBrowserAndAllTabs() {
        maybeRunOnTabObserver(observer -> observer.notifyWillDestroyBrowserAndAllTabs());
    }

    private interface OnTabObserverCallback {
        void run(ITabObserverDelegate tabObserver) throws RemoteException;
    }

    private void maybeRunOnTabObserver(OnTabObserverCallback callback) {
        if (mTabObserver != null) {
            try {
                callback.run(mTabObserver);
            } catch (RemoteException e) {
            }
        }
    }

    @Nullable
    Tab getActiveTab() {
        return mActiveTab;
    }
}
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.browserfragment.interfaces.ITabListObserverDelegate;
import org.chromium.browserfragment.interfaces.ITabParams;

/**
 * This class acts as a proxy between the TabList events happening in
 * weblayer and the BrowserObserverDelegate in browserfragment.
 */
class BrowserFragmentTabListDelegate extends TabListCallback {
    private ITabListObserverDelegate mBrowserObserver;

    private final NewTabCallback mNewTabCallback = new NewTabCallback() {
        @Override
        public void onNewTab(@NonNull Tab tab, @NewTabType int type) {
            // Set foreground tabs and tabs in new windows by default to active.
            switch (type) {
                case NewTabType.FOREGROUND_TAB:
                case NewTabType.NEW_WINDOW:
                    tab.getBrowser().setActiveTab(tab);
                    break;
            }
        }
    };

    void setObserver(ITabListObserverDelegate observer) {
        mBrowserObserver = observer;
    }

    @Override
    public void onActiveTabChanged(@Nullable Tab tab) {
        maybeRunOnBrowserObserver(observer -> {
            ITabParams tabParams = null;
            if (tab != null) {
                tabParams = TabParams.buildParcelable(tab);
            }
            observer.notifyActiveTabChanged(tabParams);
        });
    }

    @Override
    public void onTabAdded(@NonNull Tab tab) {
        // This is a requirement to open new tabs.
        tab.setNewTabCallback(mNewTabCallback);

        maybeRunOnBrowserObserver(observer -> {
            ITabParams tabParams = TabParams.buildParcelable(tab);
            observer.notifyTabAdded(tabParams);
        });
    }

    @Override
    public void onTabRemoved(@NonNull Tab tab) {
        maybeRunOnBrowserObserver(observer -> {
            ITabParams tabParams = TabParams.buildParcelable(tab);
            observer.notifyTabRemoved(tabParams);
        });
    }

    @Override
    public void onWillDestroyBrowserAndAllTabs() {
        maybeRunOnBrowserObserver(observer -> observer.notifyWillDestroyBrowserAndAllTabs());
    }

    private interface OnBrowserObserverCallback {
        void run(ITabListObserverDelegate tabObserver) throws RemoteException;
    }

    private void maybeRunOnBrowserObserver(OnBrowserObserverCallback callback) {
        if (mBrowserObserver != null) {
            try {
                callback.run(mBrowserObserver);
            } catch (RemoteException e) {
            }
        }
    }
}
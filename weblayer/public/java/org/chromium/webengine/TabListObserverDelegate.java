// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.webengine.interfaces.ITabListObserverDelegate;
import org.chromium.webengine.interfaces.ITabParams;

/**
 * {@link TabListObserverDelegate} notifies {@link TabListObserver}s of events in the Fragment.
 */
class TabListObserverDelegate extends ITabListObserverDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private ObserverList<TabListObserver> mTabListObservers = new ObserverList<TabListObserver>();

    public TabListObserverDelegate() {
        // Assert on UI thread as ObserverList can only be accessed from one thread.
        ThreadCheck.ensureOnUiThread();
    }

    /**
     * Register a TabListObserver. Call only from the UI thread.
     *
     * @return true if the observer was added to the list of observers.
     */
    boolean registerObserver(TabListObserver tabListObserver) {
        ThreadCheck.ensureOnUiThread();
        return mTabListObservers.addObserver(tabListObserver);
    }

    /**
     * Unregister a TabListObserver. Call only from the UI thread.
     *
     * @return true if the observer was removed from the list of observers.
     */
    boolean unregisterObserver(TabListObserver tabListObserver) {
        ThreadCheck.ensureOnUiThread();
        return mTabListObservers.removeObserver(tabListObserver);
    }

    @Override
    public void notifyActiveTabChanged(@Nullable ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = null;
            if (tabParams != null) {
                tab = TabRegistry.getInstance().getOrCreateTab(tabParams);
            }
            for (TabListObserver observer : mTabListObservers) {
                observer.onActiveTabChanged(tab);
            }
        });
    }

    @Override
    public void notifyTabAdded(@NonNull ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = TabRegistry.getInstance().getOrCreateTab(tabParams);
            for (TabListObserver observer : mTabListObservers) {
                observer.onTabAdded(tab);
            }
        });
    }

    @Override
    public void notifyTabRemoved(@NonNull ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = TabRegistry.getInstance().getOrCreateTab(tabParams);
            for (TabListObserver observer : mTabListObservers) {
                observer.onTabRemoved(tab);
            }
            TabRegistry.getInstance().removeTab(tab);
        });
    }

    @Override
    public void notifyWillDestroyBrowserAndAllTabs() {
        mHandler.post(() -> {
            for (TabListObserver observer : mTabListObservers) {
                observer.onWillDestroyFragmentAndAllTabs();
            }
        });
    }
}
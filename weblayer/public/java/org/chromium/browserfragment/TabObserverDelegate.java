// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.browserfragment.interfaces.ITabObserverDelegate;
import org.chromium.browserfragment.interfaces.ITabParams;

/**
 * TabObserverDelegate notifies TabObservers of Tab-events in weblayer.
 */
class TabObserverDelegate extends ITabObserverDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private ObserverList<TabObserver> mTabObservers = new ObserverList<TabObserver>();

    /**
     * Register a TabObserver.
     *
     * @return true if the observer was added to the list of observers.
     */
    boolean registerObserver(TabObserver tabObserver) {
        return mTabObservers.addObserver(tabObserver);
    }

    /**
     * Unregister a TabObserver.
     *
     * @return true if the observer was removed from the list of observers.
     */
    boolean unregisterObserver(TabObserver tabObserver) {
        return mTabObservers.removeObserver(tabObserver);
    }

    @Override
    public void notifyActiveTabChanged(@Nullable ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = null;
            if (tabParams != null) {
                tab = new Tab(tabParams);
            }
            for (TabObserver observer : mTabObservers) {
                observer.onActiveTabChanged(tab);
            }
        });
    }

    @Override
    public void notifyTabAdded(@NonNull ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = new Tab(tabParams);
            for (TabObserver observer : mTabObservers) {
                observer.onTabAdded(tab);
            }
        });
    }

    @Override
    public void notifyTabRemoved(@NonNull ITabParams tabParams) {
        mHandler.post(() -> {
            Tab tab = new Tab(tabParams);
            for (TabObserver observer : mTabObservers) {
                observer.onTabRemoved(tab);
            }
        });
    }

    @Override
    public void notifyWillDestroyBrowserAndAllTabs() {
        mHandler.post(() -> {
            for (TabObserver observer : mTabObservers) {
                observer.onWillDestroyBrowserAndAllTabs();
            }
        });
    }
}
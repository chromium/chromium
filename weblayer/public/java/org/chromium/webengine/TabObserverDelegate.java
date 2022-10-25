// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.webengine.interfaces.ITabObserverDelegate;

/**
 * {@link TabObserverDelegate} notifies registered {@Link TabObserver}s of events in the Tab.
 */
class TabObserverDelegate extends ITabObserverDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private ObserverList<TabObserver> mTabObservers = new ObserverList<TabObserver>();

    public TabObserverDelegate() {
        // Assert on UI thread as ObserverList can only be accessed from one thread.
        ThreadCheck.ensureOnUiThread();
    }

    /**
     * Registers a {@link TabObserver}. Call only from the UI thread.
     *
     * @return true if the observer was added to the list of observers.
     */
    boolean registerObserver(TabObserver tabObserver) {
        ThreadCheck.ensureOnUiThread();
        return mTabObservers.addObserver(tabObserver);
    }

    /**
     * Unregisters a {@link TabObserver}. Call only from the UI thread.
     *
     * @return true if the observer was removed from the list of observers.
     */
    boolean unregisterObserver(TabObserver tabObserver) {
        ThreadCheck.ensureOnUiThread();
        return mTabObservers.removeObserver(tabObserver);
    }

    @Override
    public void notifyVisibleUriChanged(@NonNull String uri) {
        mHandler.post(() -> {
            for (TabObserver observer : mTabObservers) {
                observer.onVisibleUriChanged(uri);
            }
        });
    }

    @Override
    public void notifyTitleUpdated(@NonNull String title) {
        mHandler.post(() -> {
            for (TabObserver observer : mTabObservers) {
                observer.onTitleUpdated(title);
            }
        });
    }

    @Override
    public void notifyRenderProcessGone() {
        mHandler.post(() -> {
            for (TabObserver observer : mTabObservers) {
                observer.onRenderProcessGone();
            }
        });
    }
}
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import androidx.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Tracks the set of Browsers.
 */
@JNINamespace("weblayer")
public final class BrowserList {
    private static BrowserList sInstance;
    private final ObserverList<BrowserListObserver> mObservers;

    @CalledByNative
    private static BrowserList createBrowserList() {
        // The native side should call this only once.
        assert sInstance == null;
        sInstance = new BrowserList();
        return sInstance;
    }

    @NonNull
    public static BrowserList getInstance() {
        // The native side creates this early on. It should never be null.
        if (sInstance == null) {
            BrowserListJni.get().createBrowserList();
            assert sInstance != null;
        }
        return sInstance;
    }

    private BrowserList() {
        mObservers = new ObserverList<>();
    }

    public void addObserver(BrowserListObserver o) {
        mObservers.addObserver(o);
    }

    public void removeObserver(BrowserListObserver o) {
        mObservers.removeObserver(o);
    }

    @CalledByNative
    private void onBrowserCreated(BrowserImpl browser) {
        for (BrowserListObserver observer : mObservers) {
            observer.onBrowserCreated(browser);
        }
    }

    @CalledByNative
    private void onBrowserDestroyed(BrowserImpl browser) {
        for (BrowserListObserver observer : mObservers) {
            observer.onBrowserDestroyed(browser);
        }
    }

    @NativeMethods
    interface Natives {
        void createBrowserList();
    }
}

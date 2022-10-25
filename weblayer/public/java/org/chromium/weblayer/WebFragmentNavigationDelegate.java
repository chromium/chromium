// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.webengine.interfaces.INavigationObserverDelegate;

/**
 * This class acts as a proxy between the Tab navigation events happening in
 * weblayer and the NavigationObserverDelegate in webengine.
 */
class WebFragmentNavigationDelegate extends NavigationCallback {
    private INavigationObserverDelegate mNavigationObserver;

    void setObserver(INavigationObserverDelegate observer) {
        mNavigationObserver = observer;
    }

    @Override
    public void onNavigationStarted(@NonNull Navigation navigation) {
        maybeRunOnNavigationObserver(observer -> {
            observer.notifyNavigationStarted(WebFragmentNavigationParams.create(navigation));
        });
    }

    @Override
    public void onNavigationRedirected(@NonNull Navigation navigation) {
        maybeRunOnNavigationObserver(observer -> {
            observer.notifyNavigationRedirected(WebFragmentNavigationParams.create(navigation));
        });
    }

    @Override
    public void onNavigationCompleted(@NonNull Navigation navigation) {
        maybeRunOnNavigationObserver(observer -> {
            observer.notifyNavigationCompleted(WebFragmentNavigationParams.create(navigation));
        });
    }

    @Override
    public void onNavigationFailed(@NonNull Navigation navigation) {
        maybeRunOnNavigationObserver(observer -> {
            observer.notifyNavigationFailed(WebFragmentNavigationParams.create(navigation));
        });
    }

    @Override
    public void onLoadProgressChanged(double progress) {
        maybeRunOnNavigationObserver(observer -> observer.notifyLoadProgressChanged(progress));
    }

    private interface OnNavigationObserverCallback {
        void run(INavigationObserverDelegate navigationObserver) throws RemoteException;
    }

    private void maybeRunOnNavigationObserver(OnNavigationObserverCallback callback) {
        if (mNavigationObserver != null) {
            try {
                callback.run(mNavigationObserver);
            } catch (RemoteException e) {
            }
        }
    }
}
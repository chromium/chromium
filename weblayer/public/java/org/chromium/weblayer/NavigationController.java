// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.INavigation;
import org.chromium.weblayer_private.interfaces.INavigationController;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.ITab;

/**
 * Provides methods to control navigation, along with maintaining the current list of navigations.
 */
public final class NavigationController {
    private INavigationController mNavigationController;
    private final ObserverList<NavigationCallback> mCallbacks;

    static NavigationController create(ITab tab) {
        NavigationController navigationController = new NavigationController();
        try {
            navigationController.mNavigationController = tab.createNavigationController(
                    navigationController.new NavigationControllerClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return navigationController;
    }

    private NavigationController() {
        mCallbacks = new ObserverList<NavigationCallback>();
    }

    public void navigate(@NonNull Uri uri) {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.navigate(uri.toString());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void goBack() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.goBack();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void goForward() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.goForward();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public boolean canGoBack() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.canGoBack();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public boolean canGoForward() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.canGoForward();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void reload() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.reload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void stop() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.stop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public int getNavigationListSize() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.getNavigationListSize();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public int getNavigationListCurrentIndex() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.getNavigationListCurrentIndex();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @NonNull
    public Uri getNavigationEntryDisplayUri(int index) {
        ThreadCheck.ensureOnUiThread();
        try {
            return Uri.parse(mNavigationController.getNavigationEntryDisplayUri(index));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void registerNavigationCallback(@NonNull NavigationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.addObserver(callback);
    }

    public void unregisterNavigationCallback(@NonNull NavigationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.removeObserver(callback);
    }

    private final class NavigationControllerClientImpl extends INavigationControllerClient.Stub {
        @Override
        public IClientNavigation createClientNavigation(INavigation navigationImpl) {
            return new Navigation(navigationImpl);
        }

        @Override
        public void navigationStarted(IClientNavigation navigation) {
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationStarted((Navigation) navigation);
            }
        }

        @Override
        public void navigationRedirected(IClientNavigation navigation) {
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationRedirected((Navigation) navigation);
            }
        }

        @Override
        public void readyToCommitNavigation(IClientNavigation navigation) {
            for (NavigationCallback callback : mCallbacks) {
                callback.onReadyToCommitNavigation((Navigation) navigation);
            }
        }

        @Override
        public void navigationCompleted(IClientNavigation navigation) {
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationCompleted((Navigation) navigation);
            }
        }

        @Override
        public void navigationFailed(IClientNavigation navigation) {
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationFailed((Navigation) navigation);
            }
        }

        @Override
        public void loadStateChanged(boolean isLoading, boolean toDifferentDocument) {
            for (NavigationCallback callback : mCallbacks) {
                callback.onLoadStateChanged(isLoading, toDifferentDocument);
            }
        }

        @Override
        public void loadProgressChanged(double progress) {
            for (NavigationCallback callback : mCallbacks) {
                callback.onLoadProgressChanged(progress);
            }
        }

        @Override
        public void onFirstContentfulPaint() {
            for (NavigationCallback callback : mCallbacks) {
                callback.onFirstContentfulPaint();
            }
        }
    }
}

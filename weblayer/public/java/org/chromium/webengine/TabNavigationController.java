// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.interfaces.ExceptionType;
import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.ITabNavigationControllerProxy;

/**
 * TabNavigationController controls the navigation in a Tab.
 */
public class TabNavigationController {
    private ITabNavigationControllerProxy mTabNavigationControllerProxy;

    private NavigationObserverDelegate mNavigationObserverDelegate;

    private final class RequestNavigationCallback extends IBooleanCallback.Stub {
        private CallbackToFutureAdapter.Completer<Boolean> mCompleter;

        RequestNavigationCallback(CallbackToFutureAdapter.Completer<Boolean> completer) {
            mCompleter = completer;
        }

        @Override
        public void onResult(boolean possible) {
            mCompleter.set(possible);
        }
        @Override
        public void onException(@ExceptionType int type, String msg) {
            mCompleter.setException(ExceptionHelper.createException(type, msg));
        }
    };

    TabNavigationController(ITabNavigationControllerProxy tabNavigationControllerProxy) {
        mTabNavigationControllerProxy = tabNavigationControllerProxy;
        mNavigationObserverDelegate = new NavigationObserverDelegate();
        try {
            mTabNavigationControllerProxy.setNavigationObserverDelegate(
                    mNavigationObserverDelegate);
        } catch (RemoteException e) {
        }
    }

    /**
     * Navigates this Tab to the given URI.
     *
     * @param uri The destination URI.
     */
    public void navigate(@NonNull String uri) {
        if (mTabNavigationControllerProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabNavigationControllerProxy.navigate(uri);
        } catch (RemoteException e) {
        }
    }

    /**
     * Navigates to the previous navigation.
     */
    public void goBack() {
        if (mTabNavigationControllerProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabNavigationControllerProxy.goBack();
        } catch (RemoteException e) {
        }
    }

    /**
     * Navigates to the next navigation.
     */
    public void goForward() {
        if (mTabNavigationControllerProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabNavigationControllerProxy.goForward();
        } catch (RemoteException e) {
        }
    }

    /**
     * Reloads this Tab. Does nothing if there are no navigations.
     */
    public void reload() {
        if (mTabNavigationControllerProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabNavigationControllerProxy.reload();
        } catch (RemoteException e) {
        }
    }

    /**
     * Stops in progress loading. Does nothing if not in the process of loading.
     */
    public void stop() {
        if (mTabNavigationControllerProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabNavigationControllerProxy.stop();
        } catch (RemoteException e) {
        }
    }

    /**
     * Returns true if there is a navigation before the current one.
     *
     * @return ListenableFuture with a Boolean stating if there is a navigation before the current
     *         one.
     */
    @NonNull
    public ListenableFuture<Boolean> canGoBack() {
        if (mTabNavigationControllerProxy == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox has been destroyed"));
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            mTabNavigationControllerProxy.canGoBack(new RequestNavigationCallback(completer));

            // Debug string.
            return "Can navigate back Future";
        });
    }

    /**
     * Returns true if there is a navigation after the current one.
     *
     * @return ListenableFuture with a Boolean stating if there is a navigation after the current
     *         one.
     */
    @NonNull
    public ListenableFuture<Boolean> canGoForward() {
        if (mTabNavigationControllerProxy == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox has been destroyed"));
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            mTabNavigationControllerProxy.canGoForward(new RequestNavigationCallback(completer));

            // Debug string.
            return "Can navigate forward Future";
        });
    }

    /**
     * Registers a {@link NavigationObserver} and returns if successful.
     *
     * @param navigationObserver The {@link NavigationObserver}.
     *
     * @return true if observer was added to the list of observers.
     */
    public boolean registerNavigationObserver(@NonNull NavigationObserver navigationObserver) {
        return mNavigationObserverDelegate.registerObserver(navigationObserver);
    }

    /**
     * Unregisters a {@link NavigationObserver} and returns if successful.
     *
     * @param navigationObserver The TabObserver to remove.
     *
     * @return true if observer was removed from the list of observers.
     */
    public boolean unregisterNavigationObserver(@NonNull NavigationObserver navigationObserver) {
        return mNavigationObserverDelegate.unregisterObserver(navigationObserver);
    }

    void invalidate() {
        mTabNavigationControllerProxy = null;
    }
}
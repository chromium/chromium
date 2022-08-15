// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.interfaces.IBooleanCallback;
import org.chromium.browserfragment.interfaces.ITabNavigationControllerProxy;

/**
 * TabNavigationController controls the navigation in a Tab.
 */
public class TabNavigationController {
    private final ITabNavigationControllerProxy mTabNavigationControllerProxy;

    private final class RequestNavigationCallback extends IBooleanCallback.Stub {
        private CallbackToFutureAdapter.Completer<Boolean> mCompleter;

        RequestNavigationCallback(CallbackToFutureAdapter.Completer<Boolean> completer) {
            mCompleter = completer;
        }

        @Override
        public void onResult(boolean possible) {
            mCompleter.set(possible);
        }
    };

    TabNavigationController(ITabNavigationControllerProxy tabNavigationControllerProxy) {
        mTabNavigationControllerProxy = tabNavigationControllerProxy;
    }

    /**
     * Navigates this Tab to the given URI.
     *
     * @param uri The destination URI.
     */
    public void navigate(@NonNull String uri) {
        try {
            mTabNavigationControllerProxy.navigate(uri);
        } catch (RemoteException e) {
        }
    }

    /**
     * Navigates to the previous navigation.
     */
    public void goBack() {
        try {
            mTabNavigationControllerProxy.goBack();
        } catch (RemoteException e) {
        }
    }

    /**
     * Navigates to the next navigation.
     */
    public void goForward() {
        try {
            mTabNavigationControllerProxy.goForward();
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
        return CallbackToFutureAdapter.getFuture(completer -> {
            mTabNavigationControllerProxy.canGoForward(new RequestNavigationCallback(completer));

            // Debug string.
            return "Can navigate forward Future";
        });
    }
}
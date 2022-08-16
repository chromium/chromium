// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.interfaces.IBooleanCallback;
import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.ITabCallback;
import org.chromium.browserfragment.interfaces.ITabParams;

/**
 * Class for interaction with Browser Tabs.
 * Calls into BrowserFragmentDelegate which runs on the Binder thread, and requires
 * finished initialization from onCreate on UIThread.
 * Access only via ListenableFuture through BrowserFragment.
 */
public class TabManager {
    private IBrowserFragmentDelegate mDelegate;

    private final class RequestNavigationCallback extends IBooleanCallback.Stub {
        private CallbackToFutureAdapter.Completer<Boolean> mCompleter;

        RequestNavigationCallback(CallbackToFutureAdapter.Completer<Boolean> completer) {
            mCompleter = completer;
        }

        @Override
        public void onResult(boolean didNavigate) {
            mCompleter.set(didNavigate);
        }
    }

    private final class TabCallback extends ITabCallback.Stub {
        private CallbackToFutureAdapter.Completer<Tab> mCompleter;

        TabCallback(CallbackToFutureAdapter.Completer<Tab> completer) {
            mCompleter = completer;
        }

        @Override
        public void onResult(@Nullable ITabParams tabParams) {
            if (tabParams != null) {
                mCompleter.set(new Tab(tabParams));
                return;
            }
            mCompleter.set(null);
        }
    };

    TabManager(IBrowserFragmentDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Returns a ListenableFuture for the currently active Tab; The tab can be null if no Tab is
     * active.
     *
     * @return ListenableFuture for the active Tab.
     */
    @NonNull
    public ListenableFuture<Tab> getActiveTab() {
        return CallbackToFutureAdapter.getFuture(completer -> {
            try {
                mDelegate.getActiveTab(new TabCallback(completer));
            } catch (RemoteException e) {
                completer.setException(e);
            }
            // Debug string.
            return "Active Tab Future";
        });
    }

    /**
     * Creates a new Tab and returns it in a ListenableFuture.
     *
     * @return ListenableFuture for the new Tab.
     */
    @NonNull
    public ListenableFuture<Tab> createTab() {
        return CallbackToFutureAdapter.getFuture(completer -> {
            try {
                mDelegate.createTab(new TabCallback(completer));
            } catch (RemoteException e) {
                completer.setException(e);
            }
            // Debug string.
            return "Create Tab Future";
        });
    }

    /**
     * Tries to navigate back inside the Browser session and returns a Future with a Boolean
     * which is true if the back navigation was successful.
     *
     * Only recommended to use if no switching of Tabs is used.
     *
     * Navigates back inside the currently active tab if possible. If that is not possible,
     * checks if any Tab was added to the BrowserFragment before the currently active Tab,
     * if so, the currently active Tab is closed and this Tab is set to active.
     *
     * @return ListenableFuture with a Boolean stating if back navigation was successful.
     */
    @NonNull
    public ListenableFuture<Boolean> tryNavigateBack() {
        return CallbackToFutureAdapter.getFuture(completer -> {
            mDelegate.tryNavigateBack(new RequestNavigationCallback(completer));
            // Debug string.
            return "Did navigate back Future";
        });
    }
}

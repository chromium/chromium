// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.ListenableFuture;

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
}

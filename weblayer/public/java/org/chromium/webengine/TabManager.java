// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Callback;
import org.chromium.webengine.interfaces.ITabCallback;
import org.chromium.webengine.interfaces.ITabManagerDelegate;
import org.chromium.webengine.interfaces.ITabParams;

import java.util.Set;

/**
 * Class for interaction with WebEngine Tabs.
 * Calls into WebEngineDelegate which runs on the Binder thread, and requires
 * finished initialization from onCreate on UIThread.
 * Access only via ListenableFuture through WebEngine.
 */
public class TabManager {
    private ITabManagerDelegate mDelegate;
    private WebEngine mWebEngine;

    private TabRegistry mTabRegistry;

    private final TabListObserverDelegate mTabListObserverDelegate;

    private Callback mInitializedTabsCallback;

    private final class TabCallback extends ITabCallback.Stub {
        private CallbackToFutureAdapter.Completer<Tab> mCompleter;

        TabCallback(CallbackToFutureAdapter.Completer<Tab> completer) {
            mCompleter = completer;
        }

        @Override
        public void onResult(@Nullable ITabParams tabParams) {
            if (tabParams != null) {
                new Handler(Looper.getMainLooper()).post(() -> {
                    mCompleter.set(mTabRegistry.getOrCreateTab(tabParams));
                });
                return;
            }
            mCompleter.set(null);
        }
    };

    TabManager(ITabManagerDelegate delegate, WebEngine webEngine) {
        mDelegate = delegate;
        mWebEngine = webEngine;
        mTabRegistry = new TabRegistry(mWebEngine);
        mTabListObserverDelegate = new TabListObserverDelegate(mWebEngine, mTabRegistry);
        try {
            mDelegate.setTabListObserverDelegate(mTabListObserverDelegate);
        } catch (RemoteException e) {
        }
    }

    void initialize(Callback<Void> initializedCallback) {
        mTabListObserverDelegate.setInitializationFinishedCallback(initializedCallback);
        try {
            mDelegate.notifyInitialTabs();
        } catch (RemoteException e) {
        }
    }

    /**
     * Registers a tab observer and returns if successful.
     *
     * @param tabListObserver The TabListObserver.
     *
     * @return true if observer was added to the list of observers.
     */
    public boolean registerTabListObserver(@NonNull TabListObserver tabListObserver) {
        return mTabListObserverDelegate.registerObserver(tabListObserver);
    }

    /**
     * Unregisters a tab observer and returns if successful.
     *
     * @param tabListObserver The TabListObserver to remove.
     *
     * @return true if observer was removed from the list of observers.
     */
    public boolean unregisterTabListObserver(@NonNull TabListObserver tabListObserver) {
        return mTabListObserverDelegate.unregisterObserver(tabListObserver);
    }

    /**
     * Returns the currently active Tab or null if no Tab is active.
     *
     * @return the active Tab.
     */
    @Nullable
    public Tab getActiveTab() {
        return mTabRegistry.getActiveTab();
    }

    /**
     * Creates a new Tab and returns it in a ListenableFuture.
     *
     * @return ListenableFuture for the new Tab.
     */
    @NonNull
    public ListenableFuture<Tab> createTab() {
        if (mDelegate == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox has been destroyed"));
        }
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

    public Set<Tab> getAllTabs() {
        return mTabRegistry.getTabs();
    }

    void invalidate() {
        mDelegate = null;
        mTabRegistry.invalidate();
    }
}

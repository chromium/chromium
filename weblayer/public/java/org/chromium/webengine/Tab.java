// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.concurrent.futures.CallbackToFutureAdapter;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.interfaces.ExceptionType;
import org.chromium.webengine.interfaces.IStringCallback;
import org.chromium.webengine.interfaces.ITabParams;
import org.chromium.webengine.interfaces.ITabProxy;
import org.chromium.webengine.interfaces.IWebMessageCallback;
import org.chromium.webengine.interfaces.IWebMessageReplyProxy;

import java.util.List;

/**
 * Tab controls the tab content and state.
 */
public class Tab {
    private ITabProxy mTabProxy;
    private TabNavigationController mTabNavigationController;
    private TabObserverDelegate mTabObserverDelegate = new TabObserverDelegate();
    private String mGuid;
    private Uri mUri = Uri.EMPTY;

    Tab(@NonNull ITabParams tabParams) {
        assert tabParams.tabProxy != null;
        assert tabParams.tabGuid != null;
        assert tabParams.navigationControllerProxy != null;

        mTabProxy = tabParams.tabProxy;
        mGuid = tabParams.tabGuid;
        mTabNavigationController =
                new TabNavigationController(tabParams.navigationControllerProxy, this);

        try {
            mTabProxy.setTabObserverDelegate(mTabObserverDelegate);
        } catch (RemoteException e) {
        }
    }

    public Uri getDisplayUri() {
        return mUri;
    }

    void setDisplayUri(Uri uri) {
        mUri = uri;
    }

    public String getGuid() {
        return mGuid;
    }

    /**
     * Sets this Tab to active.
     */
    public void setActive() {
        if (mTabProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabProxy.setActive();
        } catch (RemoteException e) {
        }
    }

    /*
     * Closes this Tab.
     */
    public void close() {
        if (mTabProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabProxy.close();
        } catch (RemoteException e) {
        }
    }

    public ListenableFuture<String> executeScript(
            @NonNull String script, boolean useSeparateIsolate) {
        if (mTabProxy == null) {
            return Futures.immediateFailedFuture(
                    new IllegalStateException("WebSandbox has been destroyed"));
        }
        return CallbackToFutureAdapter.getFuture(completer -> {
            try {
                mTabProxy.executeScript(script, useSeparateIsolate, new IStringCallback.Stub() {
                    @Override
                    public void onResult(String result) {
                        completer.set(result);
                    }
                    @Override
                    public void onException(@ExceptionType int type, String msg) {
                        completer.setException(ExceptionHelper.createException(type, msg));
                    }
                });
            } catch (RemoteException e) {
                completer.setException(e);
            }

            return "Tab.executeScript Future";
        });
    }

    /**
     * Returns the navigation controller for this Tab.
     *
     * @return The TabNavigationController.
     */
    @NonNull
    public TabNavigationController getNavigationController() {
        return mTabNavigationController;
    }

    /**
     * Adds a WebMessageCallback and injects a JavaScript object into each frame that the
     * WebMessageCallback will listen on.
     *
     * The injected JavaScript object will be named {@code jsObjectName} in the global scope. This
     * will inject the JavaScript object in any frame whose origin matches {@code
     * allowedOriginRules} for every navigation after this call, and the JavaScript object will be
     * available immediately when the page begins to load.
     */
    public void registerWebMessageCallback(
            WebMessageCallback callback, String jsObjectName, List<String> allowedOrigins) {
        if (mTabProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabProxy.registerWebMessageCallback(new IWebMessageCallback.Stub() {
                @Override
                public void onWebMessageReceived(
                        IWebMessageReplyProxy iReplyProxy, String message) {
                    callback.onWebMessageReceived(new WebMessageReplyProxy(iReplyProxy), message);
                }

                @Override
                public void onWebMessageReplyProxyClosed(IWebMessageReplyProxy iReplyProxy) {
                    callback.onWebMessageReplyProxyClosed(new WebMessageReplyProxy(iReplyProxy));
                }

                @Override
                public void onWebMessageReplyProxyActiveStateChanged(
                        IWebMessageReplyProxy iReplyProxy) {
                    callback.onWebMessageReplyProxyActiveStateChanged(
                            new WebMessageReplyProxy(iReplyProxy));
                }
            }, jsObjectName, allowedOrigins);
        } catch (RemoteException e) {
        }
    }

    /**
     * Removes the JavaScript object previously registered by way of registerWebMessageCallback.
     * This impacts future navigations (not any already loaded navigations).
     *
     * @param jsObjectName Name of the JavaScript object.
     */
    public void unregisterWebMessageCallback(String jsObjectName) {
        if (mTabProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabProxy.unregisterWebMessageCallback(jsObjectName);
        } catch (RemoteException e) {
        }
    }
    /**
     * Registers a {@link TabObserver} and returns if successful.
     *
     * @param tabObserver The TabObserver.
     *
     * @return true if observer was added to the list of observers.
     */
    public boolean registerTabObserver(@NonNull TabObserver tabObserver) {
        return mTabObserverDelegate.registerObserver(tabObserver);
    }

    /**
     * Unregisters a {@link Tabobserver} and returns if successful.
     *
     * @param tabObserver The TabObserver to remove.
     *
     * @return true if observer was removed from the list of observers.
     */
    public boolean unregisterTabObserver(@NonNull TabObserver tabObserver) {
        return mTabObserverDelegate.unregisterObserver(tabObserver);
    }

    @Override
    public int hashCode() {
        return mGuid.hashCode();
    }

    @Override
    public boolean equals(final Object obj) {
        if (obj instanceof Tab) {
            return this == obj || mGuid.equals(((Tab) obj).getGuid());
        }
        return false;
    }

    void invalidate() {
        mTabProxy = null;
        mTabNavigationController.invalidate();
    }
}

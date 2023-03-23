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

import org.chromium.base.ObserverList;
import org.chromium.webengine.interfaces.ExceptionType;
import org.chromium.webengine.interfaces.IPostMessageCallback;
import org.chromium.webengine.interfaces.IStringCallback;
import org.chromium.webengine.interfaces.ITabParams;
import org.chromium.webengine.interfaces.ITabProxy;

import java.util.List;

/**
 * Tab controls the tab content and state.
 */
public class Tab {
    private WebEngine mWebEngine;
    private ITabProxy mTabProxy;
    private TabNavigationController mTabNavigationController;
    private TabObserverDelegate mTabObserverDelegate;
    private String mGuid;
    private Uri mUri;
    private ObserverList<MessageEventListenerProxy> mMessageEventListenerProxies =
            new ObserverList<>();
    private boolean mPostMessageReady;
    private FullscreenCallbackDelegate mFullscreenCallbackDelegate;

    Tab(WebEngine webEngine, @NonNull ITabParams tabParams) {
        assert tabParams.tabProxy != null;
        assert tabParams.tabGuid != null;
        assert tabParams.navigationControllerProxy != null;
        assert tabParams.uri != null;

        mWebEngine = webEngine;
        mTabProxy = tabParams.tabProxy;
        mGuid = tabParams.tabGuid;
        mUri = Uri.parse(tabParams.uri);
        mTabObserverDelegate = new TabObserverDelegate(this);
        mTabNavigationController =
                new TabNavigationController(this, tabParams.navigationControllerProxy);
        mFullscreenCallbackDelegate = new FullscreenCallbackDelegate(mWebEngine, this);

        try {
            mTabProxy.setTabObserverDelegate(mTabObserverDelegate);
            mTabProxy.setFullscreenCallbackDelegate(mFullscreenCallbackDelegate);
        } catch (RemoteException e) {
        }
    }

    public WebEngine getWebEngine() {
        return mWebEngine;
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

    private class MessageEventListenerProxy {
        private MessageEventListener mListener;
        private List<String> mAllowedOrigins;

        private MessageEventListenerProxy(
                MessageEventListener listener, List<String> allowedOrigins) {
            mListener = listener;
            mAllowedOrigins = allowedOrigins;
        }

        private MessageEventListener getListener() {
            return mListener;
        }

        private List<String> getAllowedOrigins() {
            return mAllowedOrigins;
        }

        private void onPostMessage(String message, String origin) {
            if (!mAllowedOrigins.contains("*") && !mAllowedOrigins.contains(origin)) {
                return;
            }
            mListener.onMessage(Tab.this, message);
        }
    }

    /**
     * Add an event listener for post messages from the web content.
     * @param listener Receives the message events posted for the web content.
     * @param allowedOrigins The list of origins to accept messages from. "*" will match all
     *         origins.
     */
    public void addMessageEventListener(
            @NonNull MessageEventListener listener, @NonNull List<String> allowedOrigins) {
        if (mTabProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }

        // TODO(crbug.com/1408811): Validate the list of origins.

        MessageEventListenerProxy proxy = new MessageEventListenerProxy(listener, allowedOrigins);
        mMessageEventListenerProxies.addObserver(proxy);

        if (mPostMessageReady) {
            // We already created a message event listener in the sandbox. However we need to update
            // the list of allowedOrigins in the sandbox. This is done so that a message (with a
            // valid listener on this end) is sent over IPC once.
            try {
                mTabProxy.addMessageEventListener(proxy.getAllowedOrigins());
            } catch (RemoteException e) {
                throw new IllegalStateException("Failed to communicate with WebSandbox");
            }
            return;
        }

        IPostMessageCallback callback = new IPostMessageCallback.Stub() {
            @Override
            public void onPostMessage(String message, String origin) {
                for (MessageEventListenerProxy p : mMessageEventListenerProxies) {
                    p.onPostMessage(message, origin);
                }
            }
        };

        try {
            mTabProxy.createMessageEventListener(callback, proxy.getAllowedOrigins());
            mPostMessageReady = true;
        } catch (RemoteException e) {
            throw new IllegalStateException("Failed to communicate with WebSandbox");
        }
    }

    /**
     * Removes the event listener.
     */
    public void removeMessageEventListener(@NonNull MessageEventListener listener) {
        MessageEventListenerProxy targetProxy = null;
        for (MessageEventListenerProxy proxy : mMessageEventListenerProxies) {
            if (proxy.getListener().equals(listener)) {
                targetProxy = proxy;
                break;
            }
        }
        if (targetProxy == null) {
            return;
        }

        mMessageEventListenerProxies.removeObserver(targetProxy);
        try {
            mTabProxy.removeMessageEventListener(targetProxy.getAllowedOrigins());
        } catch (RemoteException e) {
            throw new IllegalStateException("Failed to communicate with WebSandbox");
        }
    }

    /**
     * Sends a post message to the web content. The targetOrigin must also be specified to ensure
     * the right receiver gets the message.
     *
     * To receive the message in the web page, you need to add a "message" event listener to the
     * window object.
     *
     * <pre class="prettyprint">
     * // Web page (in JavaScript):
     * window.addEventListener('message', e => {
     *   // |e.data| contains the payload.
     *   // |e.origin| contains the host app id, in the format of app://<package_name>.
     *   console.log('Received message', e.data, 'from', e.origin);
     *   // |e.ports[0]| can be used to communicate back with the host app.
     *   e.ports[0].postMessage('Received ' + e.data);
     * });
     * </pre>
     *
     * @param message The message to be sent to the web page.
     * @param targetOrigin The origin of the page that should receive the message. If '*' is
     * provided, the message will be accepted by a page of any origin.
     */
    public void postMessage(@NonNull String message, @NonNull String targetOrigin) {
        if (mTabProxy == null) {
            throw new IllegalStateException("WebSandbox has been destroyed");
        }
        try {
            mTabProxy.postMessage(message, targetOrigin);
        } catch (RemoteException e) {
        }
    }

    public void setFullscreenCallback(FullscreenCallback callback) {
        mFullscreenCallbackDelegate.setFullscreenCallback(callback);
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

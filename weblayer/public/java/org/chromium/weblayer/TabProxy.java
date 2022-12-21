// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.webengine.interfaces.ExceptionType;
import org.chromium.webengine.interfaces.IStringCallback;
import org.chromium.webengine.interfaces.ITabObserverDelegate;
import org.chromium.webengine.interfaces.ITabProxy;
import org.chromium.webengine.interfaces.IWebMessageCallback;

import java.util.List;

/**
 * This class acts as a proxy between a Tab object in the embedding app
 * and the Tab implementation in WebLayer.
 * A (@link TabProxy} is owned by the {@link Tab}.
 */
class TabProxy extends ITabProxy.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private int mTabId;
    private String mGuid;

    private WebFragmentTabDelegate mTabObserverDelegate = new WebFragmentTabDelegate();
    private WebFragmentNavigationDelegate mNavigationObserverDelegate =
            new WebFragmentNavigationDelegate();

    TabProxy(Tab tab) {
        mTabId = tab.getId();
        mGuid = tab.getGuid();

        tab.registerTabCallback(mTabObserverDelegate);
    }

    void invalidate() {
        mTabId = -1;
        mGuid = null;

        mTabObserverDelegate = null;
        mNavigationObserverDelegate = null;
    }

    boolean isValid() {
        return mGuid != null;
    }

    private Tab getTab() {
        Tab tab = Tab.getTabById(mTabId);
        if (tab == null) {
            // TODO(swestphal): Raise exception.
        }
        return tab;
    }

    @Override
    public void setActive() {
        mHandler.post(() -> {
            Tab tab = getTab();
            tab.getBrowser().setActiveTab(tab);
        });
    }

    @Override
    public void close() {
        mHandler.post(() -> {
            getTab().dispatchBeforeUnloadAndClose();
            invalidate();
        });
    }

    @Override
    public void executeScript(String script, boolean useSeparateIsolate, IStringCallback callback) {
        mHandler.post(() -> {
            try {
                getTab().executeScript(script, useSeparateIsolate,
                        new org.chromium.weblayer_private.interfaces.IStringCallback.Stub() {
                            @Override
                            public void onResult(String result) {
                                try {
                                    callback.onResult(result);
                                } catch (RemoteException e) {
                                }
                            }
                            @Override
                            public void onException(@ExceptionType int type, String msg) {
                                try {
                                    callback.onException(ExceptionHelper.convertType(type), msg);
                                } catch (RemoteException e) {
                                }
                            }
                        });
            } catch (RuntimeException e) {
                try {
                    callback.onException(ExceptionType.UNKNOWN, e.getMessage());
                } catch (RemoteException re) {
                }
            }
        });
    }

    @Override
    public void registerWebMessageCallback(
            IWebMessageCallback callback, String jsObjectName, List<String> allowedOrigins) {
        mHandler.post(() -> {
            getTab().registerWebMessageCallback(new WebMessageCallback() {
                @Override
                public void onWebMessageReceived(
                        WebMessageReplyProxy replyProxy, WebMessage message) {
                    try {
                        callback.onWebMessageReceived(
                                new WebMessageReplyProxyProxy(replyProxy), message.getContents());
                    } catch (RemoteException e) {
                    }
                }

                @Override
                public void onWebMessageReplyProxyClosed(WebMessageReplyProxy replyProxy) {
                    try {
                        callback.onWebMessageReplyProxyClosed(
                                new WebMessageReplyProxyProxy(replyProxy));
                    } catch (RemoteException e) {
                    }
                }

                @Override
                public void onWebMessageReplyProxyActiveStateChanged(
                        WebMessageReplyProxy replyProxy) {
                    try {
                        callback.onWebMessageReplyProxyActiveStateChanged(
                                new WebMessageReplyProxyProxy(replyProxy));
                    } catch (RemoteException e) {
                    }
                }
            }, jsObjectName, allowedOrigins);
        });
    }

    @Override
    public void unregisterWebMessageCallback(String jsObjectName) {
        mHandler.post(() -> { getTab().unregisterWebMessageCallback(jsObjectName); });
    }

    @Override
    public void setTabObserverDelegate(ITabObserverDelegate tabObserverDelegate) {
        mTabObserverDelegate.setObserver(tabObserverDelegate);
    }
}

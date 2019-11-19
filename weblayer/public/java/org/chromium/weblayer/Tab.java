// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Represents a single tab in a browser. More specifically, owns a NavigationController, and allows
 * configuring state of the tab, such as delegates and callbacks.
 */
public final class Tab {
    /** The top level key of the JSON object returned by executeScript(). */
    public static final String SCRIPT_RESULT_KEY = "result";

    // Maps from id (as returned from ITab.getId()) to Tab.
    private static final Map<Integer, Tab> sTabMap = new HashMap<Integer, Tab>();

    private final ITab mImpl;
    private FullscreenCallbackClientImpl mFullscreenCallbackClient;
    private final NavigationController mNavigationController;
    private final ObserverList<TabCallback> mCallbacks;
    private Browser mBrowser;
    private DownloadCallbackClientImpl mDownloadCallbackClient;
    private ErrorPageCallbackClientImpl mErrorPageCallbackClient;
    private NewTabCallback mNewTabCallback;
    // Id from the remote side.
    private final int mId;

    Tab(ITab impl, Browser browser) {
        mImpl = impl;
        mBrowser = browser;
        try {
            mId = impl.getId();
            mImpl.setClient(new TabClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }

        mCallbacks = new ObserverList<TabCallback>();
        mNavigationController = NavigationController.create(mImpl);
        registerTab(this);
    }

    static void registerTab(Tab tab) {
        assert getTabById(tab.getId()) == null;
        sTabMap.put(tab.getId(), tab);
    }

    static void unregisterTab(Tab tab) {
        assert getTabById(tab.getId()) != null;
        sTabMap.remove(tab.getId());
    }

    static Tab getTabById(int id) {
        return sTabMap.get(id);
    }

    static List<Tab> getTabsInBrowser(Browser browser) {
        List<Tab> tabs = new ArrayList<Tab>();
        for (Tab tab : sTabMap.values()) {
            if (tab.getBrowser() == browser) tabs.add(tab);
        }
        return tabs;
    }

    int getId() {
        return mId;
    }

    void setBrowser(Browser browser) {
        mBrowser = browser;
    }

    @NonNull
    public Browser getBrowser() {
        ThreadCheck.ensureOnUiThread();
        return mBrowser;
    }

    public void setDownloadCallback(@Nullable DownloadCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (callback != null) {
                mDownloadCallbackClient = new DownloadCallbackClientImpl(callback);
                mImpl.setDownloadCallbackClient(mDownloadCallbackClient);
            } else {
                mDownloadCallbackClient = null;
                mImpl.setDownloadCallbackClient(null);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setErrorPageCallback(@Nullable ErrorPageCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (callback != null) {
                mErrorPageCallbackClient = new ErrorPageCallbackClientImpl(callback);
                mImpl.setErrorPageCallbackClient(mErrorPageCallbackClient);
            } else {
                mErrorPageCallbackClient = null;
                mImpl.setErrorPageCallbackClient(null);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setFullscreenCallback(@Nullable FullscreenCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (callback != null) {
                mFullscreenCallbackClient = new FullscreenCallbackClientImpl(callback);
                mImpl.setFullscreenCallbackClient(mFullscreenCallbackClient);
            } else {
                mImpl.setFullscreenCallbackClient(null);
                mFullscreenCallbackClient = null;
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Nullable
    public DownloadCallback getDownloadCallback() {
        ThreadCheck.ensureOnUiThread();
        return mDownloadCallbackClient != null ? mDownloadCallbackClient.getCallback() : null;
    }

    /**
     * Executes the script, and returns the result as a JSON object to the callback if provided. The
     * object passed to the callback will have a single key SCRIPT_RESULT_KEY which will hold the
     * result of running the script.
     * @param useSeparateIsolate If true, runs the script in a separate v8 Isolate. This uses more
     * memory, but separates the injected scrips from scripts in the page. This prevents any
     * potentially malicious interaction between first-party scripts in the page, and injected
     * scripts. Use with caution, only pass false for this argument if you know this isn't an issue
     * or you need to interact with first-party scripts.
     */
    public void executeScript(@NonNull String script, boolean useSeparateIsolate,
            @Nullable ValueCallback<JSONObject> callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            ValueCallback<String> stringCallback = (String result) -> {
                if (callback == null) {
                    return;
                }

                try {
                    callback.onReceiveValue(
                            new JSONObject("{\"" + SCRIPT_RESULT_KEY + "\":" + result + "}"));
                } catch (JSONException e) {
                    // This should never happen since the result should be well formed.
                    throw new RuntimeException(e);
                }
            };
            mImpl.executeScript(script, useSeparateIsolate, ObjectWrapper.wrap(stringCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setNewTabCallback(@Nullable NewTabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mNewTabCallback = callback;
        try {
            mImpl.setNewTabsEnabled(mNewTabCallback != null);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Nullable
    public FullscreenCallback getFullscreenCallback() {
        ThreadCheck.ensureOnUiThread();
        return mFullscreenCallbackClient != null ? mFullscreenCallbackClient.getCallback() : null;
    }

    @NonNull
    public NavigationController getNavigationController() {
        ThreadCheck.ensureOnUiThread();
        return mNavigationController;
    }

    public void registerTabCallback(@Nullable TabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.addObserver(callback);
    }

    public void unregisterTabCallback(@Nullable TabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.removeObserver(callback);
    }

    ITab getITab() {
        return mImpl;
    }

    private final class TabClientImpl extends ITabClient.Stub {
        @Override
        public void visibleUrlChanged(String url) {
            Uri uri = Uri.parse(url);
            for (TabCallback callback : mCallbacks) {
                callback.onVisibleUrlChanged(uri);
            }
        }

        @Override
        public void onNewTab(int tabId, int mode) {
            // This should only be hit if setNewTabCallback() has been called with a non-null
            // value.
            assert mNewTabCallback != null;
            Tab tab = getTabById(tabId);
            // Tab should have already been created by way of BrowserClient.
            assert tab != null;
            assert tab.getBrowser() == getBrowser();
            mNewTabCallback.onNewTab(tab, mode);
        }

        @Override
        public void onCloseTab() {
            // This should only be hit if setNewTabCallback() has been called with a non-null
            // value.
            assert mNewTabCallback != null;
            mNewTabCallback.onCloseTab();
        }

        @Override
        public void onRenderProcessGone() {
            for (TabCallback callback : mCallbacks) {
                callback.onRenderProcessGone();
            }
        }
    }

    private static final class DownloadCallbackClientImpl extends IDownloadCallbackClient.Stub {
        private final DownloadCallback mCallback;

        DownloadCallbackClientImpl(DownloadCallback callback) {
            mCallback = callback;
        }

        public DownloadCallback getCallback() {
            return mCallback;
        }

        @Override
        public void downloadRequested(String url, String userAgent, String contentDisposition,
                String mimetype, long contentLength) {
            mCallback.onDownloadRequested(
                    url, userAgent, contentDisposition, mimetype, contentLength);
        }
    }

    private static final class ErrorPageCallbackClientImpl extends IErrorPageCallbackClient.Stub {
        private final ErrorPageCallback mCallback;

        ErrorPageCallbackClientImpl(ErrorPageCallback callback) {
            mCallback = callback;
        }

        public ErrorPageCallback getCallback() {
            return mCallback;
        }

        @Override
        public boolean onBackToSafety() {
            return mCallback.onBackToSafety();
        }
    }

    private static final class FullscreenCallbackClientImpl extends IFullscreenCallbackClient.Stub {
        private FullscreenCallback mCallback;

        /* package */ FullscreenCallbackClientImpl(FullscreenCallback callback) {
            mCallback = callback;
        }

        public FullscreenCallback getCallback() {
            return mCallback;
        }

        @Override
        public void enterFullscreen(IObjectWrapper exitFullscreenWrapper) {
            ValueCallback<Void> exitFullscreenCallback = (ValueCallback<Void>) ObjectWrapper.unwrap(
                    exitFullscreenWrapper, ValueCallback.class);
            mCallback.onEnterFullscreen(() -> exitFullscreenCallback.onReceiveValue(null));
        }

        @Override
        public void exitFullscreen() {
            mCallback.onExitFullscreen();
        }
    }
}

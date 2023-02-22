// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.IStringCallback;
import org.chromium.webengine.interfaces.IWebEngineDelegateClient;
import org.chromium.webengine.interfaces.IWebEngineParams;
import org.chromium.webengine.interfaces.IWebSandboxCallback;
import org.chromium.webengine.interfaces.IWebSandboxService;

/**
 * Implementation of IWebSandboxService.Stub to be used in in-process and out-of-process
 * browser process services.
 */
class BrowserProcessBinder extends IWebSandboxService.Stub {
    private final Context mContext;
    private WebLayer mWebLayer;

    BrowserProcessBinder(Context context) {
        mContext = context;
    }

    @Override
    public void isAvailable(IBooleanCallback callback) {
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                callback.onResult(WebLayer.isAvailable(mContext));
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void getVersion(IStringCallback callback) {
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                callback.onResult(WebLayer.getSupportedFullVersion(mContext));
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void getProviderPackageName(IStringCallback callback) {
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                callback.onResult(WebLayer.getProviderPackageName(mContext));
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void initializeBrowserProcess(IWebSandboxCallback callback) {
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                WebLayer.loadAsync(mContext, (webLayer) -> onWebLayerReady(webLayer, callback));
            } catch (Exception e) {
                try {
                    callback.onBrowserProcessInitializationFailure();
                } catch (RemoteException re) {
                }
            }
        });
    }

    private void onWebLayerReady(WebLayer webLayer, IWebSandboxCallback callback) {
        mWebLayer = webLayer;
        try {
            callback.onBrowserProcessInitialized();
        } catch (RemoteException e) {
        }
    }

    @Override
    public void createWebEngineDelegate(
            IWebEngineParams params, IWebEngineDelegateClient webEngineClient) {
        assert mWebLayer != null;

        WebEngineDelegate.create(mContext, mWebLayer, params, webEngineClient);
    }

    @Override
    public void setRemoteDebuggingEnabled(boolean enabled) {
        assert mWebLayer != null;
        mWebLayer.setRemoteDebuggingEnabled(enabled);
    }
};
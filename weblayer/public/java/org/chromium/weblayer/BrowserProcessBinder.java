// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.webengine.interfaces.IFragmentParams;
import org.chromium.webengine.interfaces.IWebFragmentDelegate;
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
    public void initializeBrowserProcess(IWebSandboxCallback callback) {
        new Handler(Looper.getMainLooper()).post(() -> {
            WebLayer.loadAsync(mContext, (webLayer) -> onWebLayerReady(webLayer, callback));
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
    public IWebFragmentDelegate createFragmentDelegate(IFragmentParams params) {
        assert mWebLayer != null;

        return new WebFragmentDelegate(mContext, mWebLayer, params);
    }

    @Override
    public void setRemoteDebuggingEnabled(boolean enabled) {
        assert mWebLayer != null;
        mWebLayer.setRemoteDebuggingEnabled(enabled);
    }
};
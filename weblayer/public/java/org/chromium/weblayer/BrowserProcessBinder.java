// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.IBrowserSandboxCallback;
import org.chromium.browserfragment.interfaces.IBrowserSandboxService;
import org.chromium.browserfragment.interfaces.IFragmentParams;

/**
 * Implementation of IBrowserSandboxService.Stub to be used in in-process and out-of-process
 * browser process services.
 */
class BrowserProcessBinder extends IBrowserSandboxService.Stub {
    private final Context mContext;
    private WebLayer mWebLayer;

    BrowserProcessBinder(Context context) {
        mContext = context;
    }

    @Override
    public void initializeBrowserProcess(IBrowserSandboxCallback callback) {
        new Handler(Looper.getMainLooper()).post(() -> {
            WebLayer.loadAsync(mContext, (webLayer) -> onWebLayerReady(webLayer, callback));
        });
    }

    private void onWebLayerReady(WebLayer webLayer, IBrowserSandboxCallback callback) {
        mWebLayer = webLayer;
        try {
            callback.onBrowserProcessInitialized();
        } catch (RemoteException e) {
        }
    }

    @Override
    public IBrowserFragmentDelegate createFragmentDelegate(IFragmentParams params) {
        assert mWebLayer != null;

        return new BrowserFragmentDelegate(mContext, mWebLayer, params);
    }

    @Override
    public void setRemoteDebuggingEnabled(boolean enabled) {
        assert mWebLayer != null;
        mWebLayer.setRemoteDebuggingEnabled(enabled);
    }
};
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.IBrowserSandboxCallback;
import org.chromium.browserfragment.interfaces.IBrowserSandboxService;
import org.chromium.browserfragment.interfaces.IFragmentParams;

/**
 * Service running the browser process for a BrowserFragment outside of the hosting
 * application's process.
 */
public class BrowserSandboxService extends Service {
    private final Context mContext = this;

    private final IBrowserSandboxService.Stub mBinder = new IBrowserSandboxService.Stub() {
        private WebLayer mWebLayer;

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

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}

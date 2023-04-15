// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.Handler;
import android.os.Looper;

import org.chromium.webengine.interfaces.IFullscreenCallbackDelegate;
import org.chromium.webengine.interfaces.IFullscreenClient;

/**
 * {@link FullscreenCallbackDelegate} notifies registered {@Link FullscreenCallback} of fullscreen
 * events of the Tab.
 */
class FullscreenCallbackDelegate extends IFullscreenCallbackDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private FullscreenCallback mFullscreenCallback;
    private WebEngine mWebEngine;
    private Tab mTab;

    public FullscreenCallbackDelegate(WebEngine webEngine, Tab tab) {
        mWebEngine = webEngine;
        mTab = tab;
    }

    public void setFullscreenCallback(FullscreenCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mFullscreenCallback = callback;
    }

    @Override
    public void onEnterFullscreen(IFullscreenClient iFullscreenClient) {
        mHandler.post(() -> {
            if (mFullscreenCallback != null) {
                mFullscreenCallback.onEnterFullscreen(
                        mWebEngine, mTab, new FullscreenClient(iFullscreenClient));
            }
        });
    }

    @Override
    public void onExitFullscreen() {
        mHandler.post(() -> {
            if (mFullscreenCallback != null) {
                mFullscreenCallback.onExitFullscreen(mWebEngine, mTab);
            }
        });
    }
}

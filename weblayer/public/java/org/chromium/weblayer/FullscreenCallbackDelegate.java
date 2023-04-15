// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.webengine.interfaces.IFullscreenCallbackDelegate;
import org.chromium.webengine.interfaces.IFullscreenClient;

/**
 * This class acts as a proxy between the Fullscreen events happening in
 * weblayer and the FullscreenCallbackDelegate in webengine.
 */
class FullscreenCallbackDelegate extends FullscreenCallback {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private IFullscreenCallbackDelegate mFullscreenCallbackDelegate;

    void setDelegate(IFullscreenCallbackDelegate delegate) {
        mFullscreenCallbackDelegate = delegate;
    }

    @Override
    public void onEnterFullscreen(@NonNull Runnable exitFullscreenRunner) {
        if (mFullscreenCallbackDelegate != null) {
            try {
                mFullscreenCallbackDelegate.onEnterFullscreen(new IFullscreenClient.Stub() {
                    @Override
                    public void exitFullscreen() {
                        mHandler.post(() -> { exitFullscreenRunner.run(); });
                    }
                });
            } catch (RemoteException e) {
            }
        }
    }

    @Override
    public void onExitFullscreen() {
        if (mFullscreenCallbackDelegate != null) {
            try {
                mFullscreenCallbackDelegate.onExitFullscreen();
            } catch (RemoteException e) {
            }
        }
    }
}

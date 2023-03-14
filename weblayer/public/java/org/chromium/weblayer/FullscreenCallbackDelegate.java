// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.webengine.interfaces.IFullscreenCallbackDelegate;

/**
 * This class acts as a proxy between the Fullscreen events happening in
 * weblayer and the FullscreenCallbackDelegate in webengine.
 */
class FullscreenCallbackDelegate extends FullscreenCallback {
    private IFullscreenCallbackDelegate mFullscreenCallbackDelegate;

    void setDelegate(IFullscreenCallbackDelegate delegate) {
        mFullscreenCallbackDelegate = delegate;
    }

    @Override
    public void onEnterFullscreen(@NonNull Runnable exitFullscreenRunner) {
        // TODO(crbug/1421742): forward exitFullscreenRunner Runnable to WebEngine if needed.
        if (mFullscreenCallbackDelegate != null) {
            try {
                mFullscreenCallbackDelegate.onEnterFullscreen();
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

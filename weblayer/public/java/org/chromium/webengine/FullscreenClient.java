// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.RemoteException;

import org.chromium.webengine.interfaces.IFullscreenClient;

/**
 * {@link FullscreenClient} is passed in {@link FullscreenCallback#onEnterFullscreen}
 * to programmatically exit fullscreen mode.
 */
public class FullscreenClient {
    private IFullscreenClient mFullscreenClient;

    FullscreenClient(IFullscreenClient client) {
        mFullscreenClient = client;
    }

    /**
     * Exit fullscreen mode, will do nothing if already closed.
     */
    public void exitFullscreen() {
        ThreadCheck.ensureOnUiThread();
        try {
            mFullscreenClient.exitFullscreen();
        } catch (RemoteException e) {
        }
    }
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Used to configure fullscreen related state. HTML fullscreen support is only enabled if a
 * FullscreenCallback is set.
 */
abstract class FullscreenCallback {
    /**
     * Called when the page has requested to go fullscreen. The delegate is responsible for
     * putting the system into fullscreen mode. The delegate can exit out of fullscreen by
     * running the supplied Runnable (calling exitFullscreenRunner.Run() results in calling
     * exitFullscreen()).
     *
     * NOTE: we expect WebLayer to be covering the whole display without any other UI elements from
     * the embedder or Android on screen. Otherwise some web features (e.g. WebXR) might experience
     * clipped or misaligned UI elements.
     *
     * NOTE: the Runnable must not be used synchronously, and must be run on the UI thread.
     */
    public abstract void onEnterFullscreen(@NonNull Runnable exitFullscreenRunner);

    /**
     * The page has exited fullscreen mode and the system should be moved out of fullscreen mode.
     */
    public abstract void onExitFullscreen();
}

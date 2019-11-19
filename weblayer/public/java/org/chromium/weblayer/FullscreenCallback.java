// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Used to configure fullscreen related state. HTML fullscreen support is only enabled if a
 * FullscreenCallback is set.
 */
public abstract class FullscreenCallback {
    /**
     * Called when the page has requested to go fullscreen. The delegate is responsible for
     *  putting the system into fullscreen mode. The delegate can exit out of fullscreen by
     * running the supplied Runnable (calling exitFullscreenRunner.Run() results in calling
     * exitFullscreen()).
     *
     * NOTE: the Runnable must not be used synchronously.
     */
    public abstract void onEnterFullscreen(@NonNull Runnable exitFullscreenRunner);

    /**
     * The page has exited fullscreen mode and the system should be moved out of fullscreen mode.
     */
    public abstract void onExitFullscreen();
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

/**
 * Fullscreen callback notifies when fullscreen mode starts and ends.
 * It expects an implementation to enter and exit fullscreen mode, e.g. expand the
 * FragmentContainerView to full size.
 */
public interface FullscreenCallback {
    /**
     /**
     * Called when the page has requested to go fullscreen. The embedder is responsible for
     * putting the system into fullscreen mode. The embedder can exit out of fullscreen by
     * running the supplied FullscreenClient by calling fullscreenClient.exitFullscreen().
     *
     * NOTE: we expect the WebEngine Fragment to be covering the whole display without any other UI
     elements from
     * the embedder or Android on screen. Otherwise some web features (e.g. WebXR) might experience
     * clipped or misaligned UI elements.
     *
     * @param webEngine the {@code WebEngine} associated with this event.
     * @param tab the {@code Tab} associated with this Event
     * @param fullscreenClient the {@code FullscreenClient} used to programmatically exit fullscreen
     mode.
     */
    public void onEnterFullscreen(WebEngine webEngine, Tab tab, FullscreenClient fullscreenClient);

    /**
     * Called when the WebEngine Tab exits fullscreen mode.
     *
     * @param webEngine the {@code WebEngine} associated with this event.
     * @param tab the {@code Tab} associated with this Event
     */
    public void onExitFullscreen(WebEngine webEngine, Tab tab);
}

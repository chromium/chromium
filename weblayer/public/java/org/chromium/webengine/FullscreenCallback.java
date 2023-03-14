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
     * Called when the WebEngine Tab enters fullscreen mode.
     *
     * @param webEngine the {@code WebEngine} associated with this event.
     * @param tab the {@code Tab} associated with this Event
     */
    public void onEnterFullscreen(WebEngine webEngine, Tab tab);

    /**
     * Called when the WebEngine Tab exits fullscreen mode.
     *
     * @param webEngine the {@code WebEngine} associated with this event.
     * @param tab the {@code Tab} associated with this Event
     */
    public void onExitFullscreen(WebEngine webEngine, Tab tab);
}

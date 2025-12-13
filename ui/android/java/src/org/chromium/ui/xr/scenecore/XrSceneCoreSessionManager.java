// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;

/**
 * This is XR scene core session management interface.
 * (https://developer.android.com/reference/androidx/xr/scenecore/package-summary.html). It's used
 * by activities to control XR space modes transitions. See implementation in {@link
 * org.chromium.chrome.browser.xr.scenecore.XrSceneCoreSessionManagerImpl}.
 */
@NullMarked
public interface XrSceneCoreSessionManager extends Destroyable {

    /**
     * Request to change XR space mode.
     *
     * @param requestFullSpaceMode True: to request Full Space mode, false to exit Full Space mode.
     * @return Success status. True: if request is handled and transition has started (the activity
     *     has focus and it's not in the middle of transition between XR space modes), false
     *     otherwise.
     */
    boolean requestSpaceModeChange(boolean requestFullSpaceMode);

    /**
     * Request to change XR space mode.
     *
     * @param requestFullSpaceMode True: to request Full Space mode, false to exit Full Space mode.
     * @param completedCallback Callback function, signaling that XR space mode transition is
     *     complete.
     * @return Success status. True: if request is handled and transition has started (the activity
     *     has focus and it's not in the middle of transition between XR space modes), false
     *     otherwise (the 'completedCallback' will not be called).
     */
    boolean requestSpaceModeChange(boolean requestFullSpaceMode, Runnable completedCallback);

    /**
     * Get XR space mode observable supplier. The supplier provides boolean value: true for XR Full
     * Space mode.
     */
    ObservableSupplier<Boolean> getXrSpaceModeObservableSupplier();

    /**
     * Is the activity in the Full Space mode. It will report the previous mode until the current
     * transition is complete.
     */
    boolean isXrFullSpaceMode();

    /** Update visibility of main panel in the Full Space mode. */
    void setMainPanelVisibility(boolean visible);
}

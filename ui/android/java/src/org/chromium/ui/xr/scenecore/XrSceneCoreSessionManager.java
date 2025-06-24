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
 *
 * <p>Usage: There are two ways to switch between XR space modes.
 *
 * <p>1. Visibility of an activity is controlled internally. Call {@link
 * XrSceneCoreSessionManager#requestSpaceModeChange}.
 *
 * <p>2. Visibility of an activity has to be controlled manually by a caller. This 3-steps flow is
 * necessary to adjust the background of the activity, hide some UI elements and avoid UI flicker.
 * Flow:
 *
 * <p>2.1. Call {@link XrSceneCoreSessionManager#startSpaceModeChange} and provide a callback. It
 * will start the XR mode transition flow and will make the activity invisible for up to 1 second.
 *
 * <p>2.2. After XR space mode transition is completed, the callback from (2.1) will be called on
 * the main thread.
 *
 * <p>2.3. The activity will still be invisible at this moment. To make it visible, finish the
 * transition by calling {@link XrSceneCoreSessionManager#finishSpaceModeChange}. All XR space mode
 * transitions requests must be called on the main thread.
 */
@NullMarked
public interface XrSceneCoreSessionManager extends Destroyable {

    /**
     * @param fsmModeRequested Requested XR space mode (true for XR full space mode).
     * @param completedCallback Callback function, signaling that XR space mode transition is
     *     completed. Caller still need to call 'finishSpaceModeChange' to make the activity
     *     visible.
     * @return Success status. True: if the activity has focus and it's not in the middle of
     *     transition between XR space modes. False: the 'completedCallback' will not be called.
     */
    boolean startSpaceModeChange(boolean fsmModeRequested, Runnable completedCallback);

    /**
     * Get XR space mode observable supplier. The supplier provides boolean value: true for XR Full
     * space mode.
     */
    ObservableSupplier<Boolean> getXrSpaceModeObservableSupplier();

    /**
     * Call to complete XR space mode transition initiated in {@link
     * XrSceneCoreSessionManager#startSpaceModeChange}.
     */
    void finishSpaceModeChange();

    /**
     * Request to change XR space mode synchronously. Visibility of the activity is controlled
     * internally.
     *
     * @param fsmModeRequested Requested XR space mode (true for XR full space mode).
     * @return success status. True: if the activity has focus and it's not in the middle of
     *     transition between XR space modes.
     */
    boolean requestSpaceModeChange(boolean fsmModeRequested);
}

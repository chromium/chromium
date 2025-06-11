// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import androidx.annotation.Nullable;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;

/**
 * This is XR scene core session manager interface. The implementation is provided by an Activity.
 * See {@link org.chromium.chrome.browser.ChromeTabbedActivity}.
 *
 * <p>Usage: 1. To switch XR space mode for an activity, the user of the interface must call
 * 'startSpaceModeChange' first. It start the mode switching flow and makes the activity invisible
 * for up to 1 second. 2. If (1) returned 'true', the provided callback will be called on the main
 * thread, signaling that the move between modes is completed. 3. The activity will be still
 * invisible at this moment. To make it visible, the 'finishSpaceModeChange' must be called.
 *
 * <p>This 3-steps flow is necessary to adjust the background of the activity, hide some UI elements
 * and avoid flicker.
 */
@NullMarked
public interface XrSceneCoreSessionManager extends Destroyable {

    /**
     * @param fsmModeRequested: requested XR space mode (true for FSM).
     * @param completedCallback: callback function, signaling that XR space mode transition is
     *     completed. It can be null, but user still need to call 'finishSpaceModeChange'.
     * @return: success status. True: if the activity has focus and it's not in the middle of
     *     transition between XR space modes. False: the 'completedCallback' will not be called.
     */
    boolean startSpaceModeChange(boolean fsmModeRequested, @Nullable Runnable completedCallback);

    /** Get XR space mode observable supplier. Returns boolean: True for XR Full space mode. */
    ObservableSupplier<Boolean> getXrSpaceModeObservableSupplier();

    /** Call to complete XR space mode transition. */
    void finishSpaceModeChange();
}

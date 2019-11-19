// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import org.chromium.base.annotations.CalledByNative;

/**
 * Simple interface allowing customized response to an overscrolling pull input.
 */
public interface OverscrollRefreshHandler {
    /**
     * Signals the start of an overscrolling pull.
     * @param type Type of the overscroll action.
     * @param startX X position of touch event at the beginning of overscroll.
     * @param startY Y position of touch event at the beginning of overscroll.
     * @param navigateForward {@code true} for forward navigation, {@code false} for back.
     *        Used only for {@link OverscrollAction.HISTORY_NAVIGATION}.
     * @return Whether the handler will consume the overscroll sequence.
     */
    @CalledByNative
    public boolean start(
            @OverscrollAction int type, float startX, float startY, boolean navigateForward);

    /**
     * Signals a pull update.
     * @param xDelta The change in horizontal pull distance (positive if pulling down, negative if
     *         up).
     * @param yDelta The change in vertical pull distance.
     */
    @CalledByNative
    public void pull(float xDelta, float yDelta);

    /**
     * Signals the release of the pull.
     * @param allowRefresh Whether the release signal should be allowed to trigger a refresh.
     */
    @CalledByNative
    public void release(boolean allowRefresh);

    /**
     * Reset the active pull state.
     */
    @CalledByNative
    public void reset();

    /**
     * Toggle whether the effect is active.
     * @param enabled Whether to enable the effect.
     *                If disabled, the effect should deactive itself apropriately.
     */
    public void setEnabled(boolean enabled);
}

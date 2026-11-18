// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/** Simple interface allowing customized response to an overscrolling pull input. */
@NullMarked
public interface OverscrollRefreshHandler {
    // LINT.IfChange
    int DEFAULT_NAVIGATION_EDGE_WIDTH = 24;

    // LINT.ThenChange(//ui/android/overscroll_refresh.h:kDefaultNavigationEdgeWidth)

    /**
     * Signals the start of an overscrolling pull.
     *
     * @param type Type of the overscroll action.
     * @param initiatingEdge Whether the history gesture is being initiated from the LEFT or RIGHT
     *     edge of the screen. Only used with the HISTORY_NAVIGATION `type`.
     * @return Whether the handler will consume the overscroll sequence.
     */
    boolean start(@OverscrollAction int type, @BackGestureEventSwipeEdge int initiatingEdge);

    /**
     * Signals a pull update.
     *
     * @param xDelta The change in horizontal pull distance (positive if pulling down, negative if
     *     up).
     * @param yDelta The change in vertical pull distance.
     */
    void pull(float xDelta, float yDelta);

    /**
     * Signals the release of the pull.
     *
     * @param status The activation status of the release gesture.
     */
    void release(@OverscrollActivationStatus int status);

    /** Reset the active pull state. */
    void reset();

    /**
     * Toggle whether the effect is active.
     *
     * @param enabled Whether to enable the effect. If disabled, the effect should deactivate itself
     *     appropriately.
     */
    void setEnabled(boolean enabled);
}

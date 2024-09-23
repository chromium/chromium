// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import org.jni_zero.CalledByNative;

import org.chromium.ui.base.BackGestureEventSwipeEdge;

/** Simple interface allowing customized response to an overscrolling pull input. */
public interface OverscrollRefreshHandler {
    /**
     * Signals the start of an overscrolling pull.
     *
     * @param type Type of the overscroll action.
     * @param initiatingEdge Whether the history gesture is being initiated from the LEFT or RIGHT
     *     edge of the screen. Only used with the HISTORY_NAVIGATION `type`. TODO(bokan): Can we
     *     make the initiatingEdge param nullable in JNI?
     * @return Whether the handler will consume the overscroll sequence.
     */
    @CalledByNative
    public boolean start(@OverscrollAction int type, @BackGestureEventSwipeEdge int initiatingEdge);

    /**
     * Signals a pull update.
     *
     * @param xDelta The change in horizontal pull distance (positive if pulling down, negative if
     *     up).
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

    /** Reset the active pull state. */
    @CalledByNative
    public void reset();

    /**
     * Toggle whether the effect is active.
     * @param enabled Whether to enable the effect.
     *                If disabled, the effect should deactive itself apropriately.
     */
    public void setEnabled(boolean enabled);
}

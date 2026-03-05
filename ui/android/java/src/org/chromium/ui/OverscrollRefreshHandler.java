// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.chromium.build.NullUtil.assertNonNull;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

import java.util.HashMap;
import java.util.Map;

/** Simple interface allowing customized response to an overscrolling pull input. */
@NullMarked
public interface OverscrollRefreshHandler {
    // Use a map to store refs to Java objects. This is necessary to avoid a ScopedJavaGlobalRef in
    // C++ of which there are only 51200 app wide. Effectively private.
    static final Map<Long, OverscrollRefreshHandler> sRefs = new HashMap<>();

    // LINT.IfChange
    int DEFAULT_NAVIGATION_EDGE_WIDTH = 24;

    // LINT.ThenChange(//ui/android/overscroll_refresh.h:kDefaultNavigationEdgeWidth)

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
    boolean start(@OverscrollAction int type, @BackGestureEventSwipeEdge int initiatingEdge);

    /**
     * Signals a pull update.
     *
     * @param xDelta The change in horizontal pull distance (positive if pulling down, negative if
     *     up).
     * @param yDelta The change in vertical pull distance.
     */
    @CalledByNative
    void pull(float xDelta, float yDelta);

    /**
     * Signals the release of the pull.
     *
     * @param allowRefresh Whether the release signal should be allowed to trigger a refresh.
     */
    @CalledByNative
    void release(boolean allowRefresh);

    /** Reset the active pull state. */
    @CalledByNative
    void reset();

    /**
     * Toggle whether the effect is active.
     *
     * @param enabled Whether to enable the effect. If disabled, the effect should deactivate itself
     *     appropriately.
     */
    void setEnabled(boolean enabled);

    @CalledByNative
    private static void setRef(long nativePtr, OverscrollRefreshHandler handler) {
        var oldValue = sRefs.put(nativePtr, handler);
        assert oldValue == null;
    }

    @CalledByNative
    private static OverscrollRefreshHandler getRef(long nativePtr) {
        return assertNonNull(sRefs.get(nativePtr));
    }

    @CalledByNative
    private static void removeRef(long nativePtr) {
        var oldValue = sRefs.remove(nativePtr);
        assert oldValue != null;
    }
}

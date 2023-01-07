// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.view.View;

/** Helper class the listen and track the latest drag event for the view. */
public interface DragStateTracker extends View.OnDragListener {
    /** Return whether there's an active drag process started. */
    default boolean isDragStarted() {
        return false;
    }

    /**
     * Return the width of the active drag shadow. Returns 0 if the tracker is not active, or an
     * active drag process is not started.
     */
    default int getDragShadowWidth() {
        return 0;
    }

    /**
     * Return the height of the active drag shadow. Returns 0 if the tracker is not active, or an
     * active drag process is not started.
     */
    default int getDragShadowHeight() {
        return 0;
    }

    /** Clean up and release the memory of this drag state tracker. */
    default void destroy() {}
}

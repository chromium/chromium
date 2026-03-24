// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/** Interface for a component that allows an XR entity to be moved by the user. */
@NullMarked
public interface XrMovableComponent {
    /**
     * Sets whether the entity is movable.
     *
     * @param movable Whether the entity should be movable.
     * @param scaleInZ Whether the entity should scale in the Z axis during movement.
     */
    void setMovable(boolean movable, boolean scaleInZ);

    /**
     * Adds a listener for movement events.
     *
     * @param listener The listener to add.
     */
    void addMoveListener(OnMoveListener listener);

    /**
     * Removes a previously added move listener.
     *
     * @param listener The listener to remove.
     */
    void removeMoveListener(OnMoveListener listener);

    /** Disposes of the movable component. */
    void dispose();

    /** Interface for listening to movement events. */
    @FunctionalInterface
    interface OnMoveListener {
        /**
         * Called when the entity's pose or scale is updated during movement.
         *
         * @param translation A 3-element array representing the X, Y, and Z translation.
         * @param rotation A 4-element array representing the X, Y, Z, and W components of the
         *     rotation quaternion.
         * @param scale The uniform scale factor applied to the entity.
         */
        void onMoveUpdate(float[] translation, float[] rotation, float scale);

        /**
         * Called when the movement starts.
         *
         * @param translation A 3-element array representing the X, Y, and Z translation.
         * @param rotation A 4-element array representing the X, Y, Z, and W components of the
         *     rotation quaternion.
         * @param scale The uniform scale factor applied to the entity.
         */
        default void onMoveStart(float[] translation, float[] rotation, float scale) {}

        /**
         * Called when the movement ends.
         *
         * @param translation A 3-element array representing the X, Y, and Z translation.
         * @param rotation A 4-element array representing the X, Y, Z, and W components of the
         *     rotation quaternion.
         * @param scale The uniform scale factor applied to the entity.
         */
        default void onMoveEnd(float[] translation, float[] rotation, float scale) {}
    }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface for a component that allows an XR entity to be moved by the user. */
@NullMarked
public interface XrMovableComponent {
    /**
     * Sets whether the entity is movable.
     *
     * <p>Note: If a custom movement handler has been configured via {@link
     * #setCustomMoveHandler(OnMoveListener)}, calling setMovable(true, ...) will reuse the custom
     * handler instead of creating a standard system movable. To use the system movable, you must
     * first clear the custom handler by calling setCustomMoveHandler(null).
     *
     * @param movable Whether the entity should be movable.
     * @param scaleInZ Whether the entity should scale in the Z axis during movement.
     */
    void setMovable(boolean movable, boolean scaleInZ);

    /**
     * Sets the entity to be movable using a custom movement handler.
     *
     * <p>Note: Setting a non-null customMoveHandler will cause subsequent calls to {@link
     * #setMovable(boolean, boolean)} (when movable is true) to reuse this custom handler instead of
     * creating a standard system movable. To use the standard system movable again, you must first
     * clear the custom handler by calling setCustomMoveHandler(null).
     *
     * @param customMoveHandler The custom movement handler that will define the movement behavior,
     *     or null to clear it.
     */
    void setCustomMoveHandler(@Nullable OnMoveListener customMoveHandler);

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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/** Interface for a component that allows an XR entity to be resized by the user. */
@NullMarked
public interface XrResizableComponent {
    /**
     * Sets the minimum allowed size for the entity.
     *
     * @param width The minimum width of the entity.
     * @param height The minimum height of the entity.
     */
    void setMinSize(float width, float height);

    /**
     * Sets the maximum allowed size for the entity.
     *
     * @param width The minimum width of the entity.
     * @param height The minimum height of the entity.
     */
    void setMaxSize(float width, float height);

    /**
     * Sets whether the entity is resizable.
     *
     * @param resizable Whether the entity should be resizable.
     * @param maintainAspectRatio Whether the entity's aspect ratio should be maintained during
     *     resizing.
     */
    void setResizable(boolean resizable, boolean maintainAspectRatio);

    /**
     * Adds a listener for resize events.
     *
     * @param listener The listener to add.
     */
    void addResizeListener(OnResizeListener listener);

    /**
     * Removes a previously added resize listener.
     *
     * @param listener The listener to remove.
     */
    void removeResizeListener(OnResizeListener listener);

    /** Disposes of the resizable component. */
    void dispose();

    /** Interface for listening to resize events. */
    @FunctionalInterface
    interface OnResizeListener {
        /**
         * Called when the entity's size is updated during resizing.
         *
         * @param width The new width of the entity.
         * @param height The new height of the entity.
         * @param depth The new depth of the entity.
         */
        void onResizeUpdate(float width, float height, float depth);

        /**
         * Called when the resizing starts.
         *
         * @param width The width of the entity when resizing started.
         * @param height The height of the entity when resizing started.
         * @param depth The depth of the entity when resizing started.
         */
        default void onResizeStart(float width, float height, float depth) {
            // No-op by default.
        }

        /**
         * Called when the resizing ends.
         *
         * @param width The final width of the entity.
         * @param height The final height of the entity.
         * @param depth The final depth of the entity.
         */
        default void onResizeEnd(float width, float height, float depth) {
            // No-op by default.
        }
    }
}

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
     * @param size The minimum size of the entity.
     */
    void setMinSize(XrFloatSize3d size);

    /** Returns the minimum allowed size for the entity. */
    XrFloatSize3d getMinSize();

    /**
     * Sets the maximum allowed size for the entity.
     *
     * @param size The maximum size of the entity.
     */
    void setMaxSize(XrFloatSize3d size);

    /** Returns the maximum allowed size for the entity. */
    XrFloatSize3d getMaxSize();

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

    /** Interface for listening to resize events. */
    @FunctionalInterface
    interface OnResizeListener {
        /**
         * Called when the entity's size is updated during resizing.
         *
         * @param size The new size of the entity.
         */
        void onResizeUpdate(XrFloatSize3d size);

        /**
         * Called when the resizing starts.
         *
         * @param size The size of the entity when resizing started.
         */
        default void onResizeStart(XrFloatSize3d size) {
            // No-op by default.
        }

        /**
         * Called when the resizing ends.
         *
         * @param size The final size of the entity.
         */
        default void onResizeEnd(XrFloatSize3d size) {
            // No-op by default.
        }
    }
}

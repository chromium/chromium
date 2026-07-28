// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/** Interface for a component that allows an XR entity to be interactable. */
@NullMarked
public interface XrInteractableComponent {
    /** Interface for listening to click events. */
    @FunctionalInterface
    interface OnClickListener {
        /** Called when the entity is clicked. */
        void onClick();
    }

    /** Interface for listening to drag events. */
    interface OnDragListener {
        /** Called when a drag gesture starts. */
        void onDragStart(XrVector3 origin, XrVector3 direction);

        /** Called when a drag gesture is updated. */
        void onDragUpdate(XrVector3 origin, XrVector3 direction);

        /** Called when a drag gesture ends. */
        void onDragEnd(XrVector3 origin, XrVector3 direction);
    }

    /**
     * Sets whether the entity is interactable.
     *
     * @param interactable Whether the entity should be interactable.
     */
    void setInteractable(boolean interactable);

    /**
     * Adds a listener for click events.
     *
     * @param listener The listener to add.
     */
    void addOnClickListener(OnClickListener listener);

    /**
     * Removes a previously added click listener.
     *
     * @param listener The listener to remove.
     */
    void removeOnClickListener(OnClickListener listener);

    /**
     * Adds a listener for drag events.
     *
     * @param listener The listener to add.
     */
    void addOnDragListener(OnDragListener listener);

    /**
     * Removes a previously added drag listener.
     *
     * @param listener The listener to remove.
     */
    void removeOnDragListener(OnDragListener listener);
}

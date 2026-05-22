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
}

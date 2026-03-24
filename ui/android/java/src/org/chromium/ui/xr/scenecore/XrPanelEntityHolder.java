// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import android.util.Size;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for a class that holds an XR panel entity. Panels are typically used to display 2D
 * content in an XR scene.
 *
 * @param <EntityType> The type of the underlying XR entity.
 */
@NullMarked
public interface XrPanelEntityHolder<EntityType>
        extends XrEntityHolder<EntityType>, XrMovableEntityHolder, XrResizableEntityHolder {

    /** Returns the size of the panel in pixels. */
    Size getEntitySizeInPixels();

    /**
     * Sets the size of the panel in pixels.
     *
     * @param width The width of the panel in pixels.
     * @param height The height of the panel in pixels.
     */
    void setEntitySizeInPixels(int width, int height);

    /** Returns the corner radius of the panel. */
    float getEntityCornerRadius();

    /**
     * Sets the corner radius of the panel.
     *
     * @param radius The corner radius of the panel.
     */
    void setEntityCornerRadius(float radius);
}

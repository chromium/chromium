// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for a class that holds a curved XR surface entity.
 *
 * @param <EntityType> The type of the underlying XR entity.
 */
@NullMarked
public interface XrCurvedSurfaceEntityHolder<EntityType> extends XrSurfaceEntityHolder<EntityType> {
    /**
     * Sets the radius of the curved surface.
     *
     * @param radius The radius of the curved surface in meters.
     */
    void setEntityRadius(float radius);

    /** Returns the radius of the curved surface. */
    float getEntityRadius();
}

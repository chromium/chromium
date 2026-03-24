// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for a class that holds an XR entity. It provides methods to manipulate the entity's
 * properties and lifecycle.
 *
 * @param <EntityType> The type of the underlying XR entity.
 */
@NullMarked
public interface XrEntityHolder<EntityType> {
    /** Returns the underlying XR entity. */
    EntityType getEntity();

    /**
     * Sets the entity's translation in the XR space.
     *
     * @param translation A 3-element array representing the X, Y, and Z translation.
     */
    void setEntityPose(float[] translation);

    /**
     * Sets the entity's translation and rotation in the XR space.
     *
     * @param translation A 3-element array representing the X, Y, and Z translation.
     * @param rotation A 4-element array representing the X, Y, Z, and W components of the rotation
     *     quaternion.
     */
    void setEntityPose(float[] translation, float[] rotation);

    /** Returns the entity's translation in the XR space. */
    float[] getEntityTranslation();

    /** Returns the entity's rotation in the XR space. */
    float[] getEntityRotation();

    /**
     * Sets the entity's scale.
     *
     * @param scale The uniform scale factor to apply to the entity.
     */
    void setEntityScale(float scale);

    /** Returns the entity's scale. */
    float getEntityScale();

    /**
     * Sets the entity's alpha (transparency).
     *
     * @param alpha The alpha value (0.0 to 1.0) to apply to the entity.
     */
    void setEntityAlpha(float alpha);

    /** Returns the entity's alpha (transparency). */
    float getEntityAlpha();

    /**
     * Sets whether the entity is enabled and visible in the scene.
     *
     * @param enabled Whether the entity should be enabled and visible.
     */
    void setEntityEnabled(boolean enabled);

    /** Returns whether the entity is enabled. */
    boolean isEntityEnabled();

    /** Disposes of the entity and releases associated resources. */
    void dispose();
}

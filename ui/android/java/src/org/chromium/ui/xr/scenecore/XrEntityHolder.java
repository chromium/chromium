// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
     * Sets the entity's translation in the specified XR space.
     *
     * @param translation A 3-element array representing the X, Y, and Z translation.
     * @param space The space to pose the entity in.
     */
    void setEntityPose(float[] translation, @XrSpace int space);

    /**
     * Sets the entity's translation and rotation in the specified XR space.
     *
     * @param translation A 3-element array representing the X, Y, and Z translation.
     * @param rotation A 4-element array representing the X, Y, Z, and W components of the rotation
     *     quaternion.
     * @param space The space to pose the entity in.
     */
    void setEntityPose(float[] translation, float[] rotation, @XrSpace int space);

    /**
     * Returns the entity's translation in the specified XR space.
     *
     * @param space The space to query the translation in.
     */
    float[] getEntityTranslation(@XrSpace int space);

    /**
     * Returns the entity's rotation in the specified XR space.
     *
     * @param space The space to query the rotation in.
     */
    float[] getEntityRotation(@XrSpace int space);

    /**
     * Sets the entity's scale in the specified XR space.
     *
     * @param scale The uniform scale factor to apply to the entity.
     * @param space The space to set the scale in.
     */
    void setEntityScale(float scale, @XrSpace int space);

    /**
     * Returns the entity's scale in the specified XR space.
     *
     * @param space The space to query the scale in.
     */
    float getEntityScale(@XrSpace int space);

    /**
     * Sets the entity's alpha (transparency).
     *
     * @param alpha The alpha value (0.0 to 1.0) to apply to the entity.
     */
    void setEntityAlpha(float alpha);

    /**
     * Returns the entity's alpha (transparency) in the specified XR space.
     *
     * @param space The space to query the alpha in.
     */
    float getEntityAlpha(@XrSpace int space);

    /**
     * Sets whether the entity is enabled and visible in the scene.
     *
     * @param enabled Whether the entity should be enabled and visible.
     */
    void setEntityEnabled(boolean enabled);

    /** Returns whether the entity is enabled. */
    boolean isEntityEnabled();

    /**
     * Adds a child entity to this entity.
     *
     * @param child The child entity to add.
     */
    void addChild(XrEntityHolder<?> child);

    /**
     * Sets the parent of this entity.
     *
     * @param parent The new parent entity, or null to detach from parent.
     */
    void setParent(@Nullable XrEntityHolder<?> parent);

    /** Returns the parent of this entity, or null if it has no parent. */
    @Nullable XrEntityHolder<?> getParent();

    /** Disposes of the entity and releases associated resources. */
    void dispose();

    /** Returns true if the entity has been disposed, false otherwise. */
    boolean isDisposed();
}

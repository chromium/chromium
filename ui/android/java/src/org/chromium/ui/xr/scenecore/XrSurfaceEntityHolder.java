// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import android.view.Surface;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Interface for a class that holds an XR surface entity. It provides access to the underlying
 * {@link Surface} and methods to manage it.
 *
 * @param <EntityType> The type of the underlying XR entity.
 */
@NullMarked
public interface XrSurfaceEntityHolder<EntityType> extends XrTransformableEntityHolder<EntityType> {
    /**
     * Adds a callback to be notified of surface lifecycle events.
     *
     * @param callback The callback to add.
     */
    void addCallback(Callback callback);

    /**
     * Removes a previously added callback.
     *
     * @param callback The callback to remove.
     */
    void removeCallback(Callback callback);

    /** Returns the underlying {@link Surface}, or null if it's not available. */
    @Nullable Surface getSurface();

    /** Returns the stereo mode for the surface. */
    @XrSurfaceEntityStereoMode
    int getSurfaceStereoMode();

    /**
     * Sets the stereo mode for the surface.
     *
     * @param stereoMode The new stereo mode for the surface.
     */
    void setSurfaceStereoMode(@XrSurfaceEntityStereoMode int stereoMode);

    /** Returns the shape of the surface. */
    @XrSurfaceEntityShape
    int getSurfaceShape();

    /**
     * Sets the shape of the surface.
     *
     * @param shape The new shape for the surface.
     */
    void setSurfaceShape(@XrSurfaceEntityShape int shape);

    /**
     * Sets a custom mesh for the surface shape. This is used when the surface shape is set to
     * {@link XrSurfaceEntityShape#CUSTOM}.
     *
     * @param meshDatas The mesh data to apply.
     */
    void setSurfaceShape(XrMeshData[] meshDatas);

    /**
     * Sets the pixel dimensions of the surface.
     *
     * @param width The new width of the surface in pixels.
     * @param height The new height of the surface in pixels.
     */
    void setSurfacePixelDimensions(int width, int height);

    /** Callback interface for surface lifecycle events. */
    interface Callback {
        /**
         * Called when the surface is created.
         *
         * @param surface The newly created {@link Surface}.
         */
        void surfaceCreated(Surface surface);

        /**
         * Called when the surface dimensions change.
         *
         * @param surface The {@link Surface} whose dimensions changed.
         * @param width The new width of the surface in pixels.
         * @param height The new height of the surface in pixels.
         */
        void surfaceChanged(Surface surface, int width, int height);

        /** Called when the surface is destroyed. */
        void surfaceDestroyed();
    }
}

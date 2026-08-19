// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This is XR scene core session management interface.
 * (https://developer.android.com/reference/androidx/xr/scenecore/package-summary.html). It's used
 * by activities to control XR space modes transitions. See implementation in {@link
 * org.chromium.chrome.browser.xr.scenecore.XrSceneCoreSessionManagerImpl}.
 */
@NullMarked
public interface XrSceneCoreSessionManager extends Destroyable {

    /**
     * Request to change XR space mode.
     *
     * @param requestFullSpaceMode True: to request Full Space mode, false to exit Full Space mode.
     * @return Success status. True: if request is handled and transition has started (the activity
     *     has focus and it's not in the middle of transition between XR space modes), false
     *     otherwise.
     */
    boolean requestSpaceModeChange(boolean requestFullSpaceMode);

    /**
     * Request to change XR space mode.
     *
     * @param requestFullSpaceMode True: to request Full Space mode, false to exit Full Space mode.
     * @param completedCallback Callback function, signaling that XR space mode transition is
     *     complete.
     * @return Success status. True: if request is handled and transition has started (the activity
     *     has focus and it's not in the middle of transition between XR space modes), false
     *     otherwise (the 'completedCallback' will not be called).
     */
    boolean requestSpaceModeChange(boolean requestFullSpaceMode, Runnable completedCallback);

    /**
     * Get XR space mode observable supplier. The supplier provides boolean value: true for XR Full
     * Space mode.
     */
    NonNullObservableSupplier<Boolean> getXrSpaceModeObservableSupplier();

    /**
     * Is the activity in the Full Space mode. It will report the previous mode until the current
     * transition is complete.
     */
    boolean isXrFullSpaceMode();

    /** Update visibility of main panel in the Full Space mode. */
    void setMainPanelVisibility(boolean visible);

    /**
     * Creates an XR surface entity with the specified shape.
     *
     * @param shape The shape of the surface entity (see {@link XrSurfaceEntityShape}).
     * @return An {@link XrSurfaceEntityHolder} for the created surface.
     */
    XrSurfaceEntityHolder createSurfaceEntity(@XrSurfaceEntityShape int shape);

    /**
     * Creates an XR panel entity from an Android view.
     *
     * @param view The Android view to be hosted in the panel.
     * @param name A name for the panel entity (used for debugging/identification).
     * @return An {@link XrPanelEntityHolder} for the created panel.
     */
    XrPanelEntityHolder createPanelEntity(View view, String name);

    /** Returns the {@link XrPanelEntityHolder} for the main panel of the session. */
    XrPanelEntityHolder getMainPanelEntity();

    /** Returns the {@link XrEntityHolder} for the Activity Space. */
    XrEntityHolder getActivitySpaceEntity();

    /**
     * Sets the key entity for the session. This is typically used to identify the entity that is
     * the primary focus of the user's interaction.
     *
     * @param entityHolder The entity to be set as the key entity.
     */
    void setKeyEntity(@Nullable XrEntityHolder entityHolder);

    /** Returns the user's head pose in the Activity Space, or null if tracking is unavailable. */
    @Nullable XrPose getHeadPoseInActivitySpace();

    /**
     * Enables or disables head tracking for the session.
     *
     * @param enable True to enable head tracking, false to disable.
     */
    void setHeadTrackingEnabled(boolean enable);

    /** Returns whether head tracking is enabled for the session. */
    boolean isHeadTrackingEnabled();

    /**
     * Starts head pose tracking.
     *
     * @return True if head pose tracking was successfully started, false otherwise.
     */
    boolean startHeadPoseTracking();

    /** Stops head pose tracking. */
    void stopHeadPoseTracking();

    /** Returns the head pose observable supplier. */
    NullableObservableSupplier<XrPose> getHeadPoseObservableSupplier();

    /** Returns the {@link XrPixelDensity} for the session. */
    XrPixelDensity getPixelDensity();
}

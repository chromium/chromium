// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A singleton utility class to manages XR session and UI environment. */
@NullMarked
public class XrUtils {

    private static XrUtils sInstance = new XrUtils();
    private static @Nullable Boolean sXrDeviceOverrideForTesting;

    // For spatialization of Chrome app using Jetpack XR.
    private boolean mXrInitialized;
    private boolean mInFullSpaceMode;

    /** Returns the singleton instance of XRUtils. */
    public static XrUtils getInstance() {
        return sInstance;
    }

    /** Set the XR device envornment for testing. */
    public static void setXrDeviceForTesting(boolean isXrDevice) {
        sXrDeviceOverrideForTesting = isXrDevice;
    }

    /** Reset the XR device envornment to the default value. */
    public static void resetXrDeviceForTesting() {
        sXrDeviceOverrideForTesting = null;
    }

    /** Return if the app is running on an immersive XR device. */
    public static boolean isXrDevice() {
        boolean xrDevice =
                (sXrDeviceOverrideForTesting != null)
                        ? sXrDeviceOverrideForTesting
                        : PackageManagerUtils.hasSystemFeature(
                                PackageManagerUtils.XR_IMMERSIVE_FEATURE_NAME);
        return xrDevice;
    }

    /**
     * Initialize the class and store info that will be used during spatialization and maintaining
     * viewing state.
     */
    public void init(@NonNull Activity activity) {
        // Note: The activity/window handle is only used in the function during initialization of XR
        // and not saved in the class to prevent activity leaks.
        if (!isXrDevice()) return;

        // Initialization of XR for spatialization will occur here using JXR.
        mXrInitialized = true;
    }

    /**
     * Initialize viewing of the XR environment in the full space mode in which only the single
     * activity is visible to the user and all other activities are hidden out.
     */
    public void viewInFullSpaceMode() {
        if (!mXrInitialized) return;
        // Requesting of full space mode will occur here using JXR.
        mInFullSpaceMode = true;
    }

    /**
     * Initiate viewing of the XR environment in the default home space mode in which all open
     * activity is visible to the user similar to desktop environment.
     */
    public void viewInHomeSpaceMode() {
        if (!mXrInitialized) return;
        // Requesting of home space mode will occur here using JXR.
        mInFullSpaceMode = false;
    }

    /**
     * Check if the device is in full space mode or in home space mode. Only one of these mode is
     * active at any given time.
     */
    public boolean isFsmOnXrDevice() {
        return isXrDevice() && mInFullSpaceMode;
    }

    boolean isXrInitializedForTesting() {
        return mXrInitialized;
    }

    public static void setXrUtilsForTesting() {
        XrUtils oldInstance = sInstance;
        sInstance = new XrUtils();
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }
}

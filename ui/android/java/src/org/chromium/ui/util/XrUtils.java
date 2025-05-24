// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

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
    private boolean mInFullSpaceMode;

    /** Returns the singleton instance of XRUtils. */
    public static XrUtils getInstance() {
        return sInstance;
    }

    /** Set the XR device environment for testing. */
    public static void setXrDeviceForTesting(Boolean isXrDevice) {
        sXrDeviceOverrideForTesting = isXrDevice;
        ResettersForTesting.register(() -> sXrDeviceOverrideForTesting = null);
    }

    /** Return if the app is running on an immersive XR device. */
    public static boolean isXrDevice() {
        boolean xrDevice =
                (sXrDeviceOverrideForTesting != null)
                        ? sXrDeviceOverrideForTesting
                        : (PackageManagerUtils.hasSystemFeature(
                                        PackageManagerUtils.XR_IMMERSIVE_FEATURE_NAME)
                                || PackageManagerUtils.hasSystemFeature(
                                        PackageManagerUtils.XR_OPENXR_FEATURE_NAME));
        return xrDevice;
    }

    /**
     * Check if the device is in full space mode or in home space mode. Only one of these mode is
     * active at any given time.
     */
    public boolean isFsmOnXrDevice() {
        return isXrDevice() && mInFullSpaceMode;
    }

    /** Set the current full space mode based of the Android XR. */
    public void setFullSpaceMode(boolean fullSpaceMode) {
        if (!isXrDevice()) return;
        mInFullSpaceMode = fullSpaceMode;
    }

    /** Determine if the Android XR is in full space mode. */
    public boolean getFullSpaceMode() {
        return mInFullSpaceMode;
    }

    public static void setXrUtilsForTesting(XrUtils instance) {
        XrUtils oldInstance = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }
}

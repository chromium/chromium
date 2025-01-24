// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import org.chromium.base.PackageManagerUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A singleton utility class to manages XR session and UI environment. */
@NullMarked
public class XrUtils {

    private static XrUtils sInstance = new XrUtils();
    private static @Nullable Boolean sXrDeviceOverrideForTesting;

    protected XrUtils() {}

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
}

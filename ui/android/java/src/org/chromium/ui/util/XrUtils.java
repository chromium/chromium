// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** An utility class to manages XR session and UI environment. */
@NullMarked
public class XrUtils {
    private static @Nullable Boolean sXrDeviceOverrideForTesting;
    private static @Nullable Boolean sIsXrDevice;

    /** Set the XR device environment for testing. */
    public static void setXrDeviceForTesting(Boolean isXrDevice) {
        sXrDeviceOverrideForTesting = isXrDevice;
        ResettersForTesting.register(() -> sXrDeviceOverrideForTesting = null);
    }

    /** Return if the app is running on an immersive XR device. */
    public static boolean isXrDevice() {
        if (sIsXrDevice == null) sIsXrDevice = isXrDeviceInternal();
        return sXrDeviceOverrideForTesting != null ? sXrDeviceOverrideForTesting : sIsXrDevice;
    }

    private static boolean isXrDeviceInternal() {
        return PackageManagerUtils.hasSystemFeature(PackageManagerUtils.XR_IMMERSIVE_FEATURE_NAME)
                || PackageManagerUtils.hasSystemFeature(PackageManagerUtils.XR_OPENXR_FEATURE_NAME);
    }
}

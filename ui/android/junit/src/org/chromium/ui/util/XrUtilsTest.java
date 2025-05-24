// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.Build.VERSION_CODES;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link XrUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.UPSIDE_DOWN_CAKE)
public class XrUtilsTest {

    @Test
    public void getInstanceTest_notNull() {
        // Verify test the instance is created.
        assertNotNull("XrUtils instance is missing.", XrUtils.getInstance());
    }

    @Test
    public void isFsmOnXrDeviceTest_xrSpatialModeNotSetOnNonXrDevice() {
        // Test
        XrUtils.setXrDeviceForTesting(false);
        XrUtils.getInstance().setFullSpaceMode(true);

        // Verify the XR spatial mode can't be set.
        assertFalse(
                "The XR device can't be in spatial mode .",
                XrUtils.getInstance().isFsmOnXrDevice());
    }

    @Test
    public void isFsmOnXrDeviceTest_xrSpatialModeSetOnXrDevice() {
        // Test
        XrUtils.setXrDeviceForTesting(true);
        XrUtils.getInstance().setFullSpaceMode(true);
        // Verify the XR spatial mode can be set.
        assertTrue(
                "The XR device should be in spatial mode.",
                XrUtils.getInstance().isFsmOnXrDevice());
    }

    @Test
    public void getFullSpaceModeTest_OnXrDevice_isTrue() {
        // Test
        XrUtils.setXrDeviceForTesting(true);
        XrUtils.getInstance().setFullSpaceMode(true);
        // Verify if the XR full sapce mode is set.
        assertTrue(
                "The XR device should be in full space mode.",
                XrUtils.getInstance().getFullSpaceMode());
    }
}

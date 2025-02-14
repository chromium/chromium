// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link XrUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrUtilsTest {

    private Activity mActivity;

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        XrUtils.resetXrDeviceForTesting();
        XrUtils.setXrUtilsForTesting();
    }

    @Test
    public void getInstanceTest_notNull() {
        // Verify test the instance is created.
        assertNotNull("XrUtils instance is missing.", XrUtils.getInstance());
    }

    @Test
    public void initTest_xrNotInitializedOnNonXrDevice() {
        // Test
        XrUtils.setXrDeviceForTesting(false);
        XrUtils.getInstance().init(mActivity);

        // Verify the XR is not initialized.
        assertFalse(
                "The XR should not be initialized.",
                XrUtils.getInstance().isXrInitializedForTesting());
    }

    @Test
    public void initTest_xrIsInitalizedOnXrDevice() {
        // Test
        XrUtils.setXrDeviceForTesting(true);
        XrUtils.getInstance().init(mActivity);

        // Verify the xrInitialized is set.
        assertTrue(
                "The xr should be initialized.", XrUtils.getInstance().isXrInitializedForTesting());
    }

    @Test
    public void viewInFullSpaceModeTest_isTrue() {
        // Test
        XrUtils.setXrDeviceForTesting(true);
        XrUtils.getInstance().init(mActivity);
        XrUtils.getInstance().viewInFullSpaceMode();

        // Verify the full space mode is set.
        assertTrue("The FSM  mode should be true.", XrUtils.getInstance().isFsmOnXrDevice());
    }

    @Test
    public void viewInFullSpaceModeTest_isFalse() {
        // Test
        XrUtils.setXrDeviceForTesting(false);
        XrUtils.getInstance().init(mActivity);

        // Verify the home space mode is always off by default
        assertFalse("The FSM mode should be false.", XrUtils.getInstance().isFsmOnXrDevice());

        // Test
        XrUtils.setXrDeviceForTesting(true);
        XrUtils.getInstance().viewInHomeSpaceMode();

        // Verify the home space mode is set.
        assertFalse("The FSM mode should be false.", XrUtils.getInstance().isFsmOnXrDevice());
    }
}

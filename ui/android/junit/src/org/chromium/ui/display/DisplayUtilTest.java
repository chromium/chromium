// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.display;

import static org.junit.Assert.assertEquals;

import android.util.DisplayMetrics;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests logic in the {@link DisplayUtil} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class DisplayUtilTest {
    @Test
    public void testScaleUpDisplayMetricsForAutomotive() {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = 1.0f;
        displayMetrics.densityDpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.xdpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.ydpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.widthPixels = 100;
        displayMetrics.heightPixels = 100;

        int scaledUpDensity = (int) (DisplayMetrics.DENSITY_DEFAULT
                * DisplayUtil.getUiScalingFactorForAutomotive());
        DisplayUtil.scaleUpDisplayMetricsForAutomotive(displayMetrics);
        assertEquals("The DisplayMetrics density should be scaled up by the "
                        + "automotive scale-up factor.",
                DisplayUtil.getUiScalingFactorForAutomotive(), displayMetrics.density, 0.1f);
        assertEquals("The DisplayMetrics densityDpi should be scaled up by the "
                        + "automotive scale-up factor.",
                scaledUpDensity, displayMetrics.densityDpi);
        assertEquals("The DisplayMetrics xdpi should be scaled up by the "
                        + "automotive scale-up factor.",
                scaledUpDensity, (int) displayMetrics.xdpi);
        assertEquals("The DisplayMetrics ydpi should be scaled up by the "
                        + "automotive scale-up factor.",
                scaledUpDensity, (int) displayMetrics.ydpi);
        assertEquals("The DisplayMetrics widthPixels should not be affected by the "
                        + "automotive scale-up factor.",
                100, displayMetrics.widthPixels);
        assertEquals("The DisplayMetrics heightPixels should not be affected by the "
                        + "automotive scale-up factor.",
                100, displayMetrics.heightPixels);
    }
}

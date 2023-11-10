// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.display;

import static org.junit.Assert.assertEquals;

import android.util.DisplayMetrics;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests logic in the {@link DisplayUtil} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class DisplayUtilTest {
    @Test
    public void testGetUiDensityForAutomotive() {
        assertEquals(
                "Density 140 should be scaled to 187.6 and adjusted up to Density 200.",
                DisplayMetrics.DENSITY_200,
                DisplayUtil.getUiDensityForAutomotive(DisplayMetrics.DENSITY_140));
        assertEquals(
                "Density 160 should be scaled to 214.4 and adjusted up to Density 220.",
                DisplayMetrics.DENSITY_220,
                DisplayUtil.getUiDensityForAutomotive(DisplayMetrics.DENSITY_DEFAULT));
        assertEquals(
                "Density 180 should be scaled to 241.2 and adjusted up to Density 260.",
                DisplayMetrics.DENSITY_260,
                DisplayUtil.getUiDensityForAutomotive(DisplayMetrics.DENSITY_180));
    }

    @Test
    public void testScaleUpDisplayMetricsForAutomotive() {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = 1.0f;
        displayMetrics.densityDpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.xdpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.ydpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.widthPixels = 100;
        displayMetrics.heightPixels = 100;

        DisplayUtil.scaleUpDisplayMetricsForAutomotive(displayMetrics);
        assertEquals(
                "The DisplayMetrics density should be scaled up by the "
                        + "automotive scale-up factor.",
                DisplayUtil.getUiScalingFactorForAutomotive(),
                displayMetrics.density,
                0.1f);
        assertEquals(
                "The DisplayMetrics densityDpi should be scaled up by the "
                        + "automotive scale-up factor.",
                DisplayMetrics.DENSITY_220,
                displayMetrics.densityDpi);
        assertEquals(
                "The DisplayMetrics xdpi should be scaled up by the "
                        + "automotive scale-up factor.",
                DisplayMetrics.DENSITY_220,
                (int) displayMetrics.xdpi);
        assertEquals(
                "The DisplayMetrics ydpi should be scaled up by the "
                        + "automotive scale-up factor.",
                DisplayMetrics.DENSITY_220,
                (int) displayMetrics.ydpi);
        assertEquals(
                "The DisplayMetrics widthPixels should not be affected by the "
                        + "automotive scale-up factor.",
                100,
                displayMetrics.widthPixels);
        assertEquals(
                "The DisplayMetrics heightPixels should not be affected by the "
                        + "automotive scale-up factor.",
                100,
                displayMetrics.heightPixels);
    }
}

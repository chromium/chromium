// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.display;

import static org.junit.Assert.assertEquals;

import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.WindowInsets;
import android.view.WindowMetrics;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Tests logic in the {@link DisplayUtil} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class DisplayUtilTest {

    @Implements(WindowMetrics.class)
    public static class ShadowWindowMetrics {
        public ShadowWindowMetrics() {}

        @Implementation
        protected void __constructor__(Rect bounds, WindowInsets windowInsets, float density) {
            // Leave blank to avoid creating unnecessary objects.
        }

        @Implementation
        public static WindowInsets getWindowInsets() {
            return DisplayUtilTest.TEST_WINDOW_INSETS;
        }
    }

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private static final int TEST_STATUS_BAR_HEIGHT = 100;
    private static final int TEST_NAVIGATION_BAR_HEIGHT = 100;
    private static final int TEST_LEFT_INSET = 100;
    private static final int TEST_RIGHT_INSET = 100;
    private static final Insets TEST_SYSTEM_BAR_INSETS =
            Insets.of(
                    TEST_LEFT_INSET,
                    TEST_STATUS_BAR_HEIGHT,
                    TEST_RIGHT_INSET,
                    TEST_NAVIGATION_BAR_HEIGHT);
    private static final WindowInsets TEST_WINDOW_INSETS =
            new WindowInsets.Builder()
                    .setInsets(WindowInsets.Type.systemBars(), TEST_SYSTEM_BAR_INSETS)
                    .build();

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testGetUiDensityForAutomotive() {
        assertEquals(
                "Density 140 should be scaled to 187.6 and adjusted up to Density 200.",
                DisplayMetrics.DENSITY_200,
                DisplayUtil.getUiDensityForAutomotive(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_140));
        assertEquals(
                "Density 160 should be scaled to 214.4 and adjusted up to Density 220.",
                DisplayMetrics.DENSITY_220,
                DisplayUtil.getUiDensityForAutomotive(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_DEFAULT));
        assertEquals(
                "Density 180 should be scaled to 241.2 and adjusted up to Density 260.",
                DisplayMetrics.DENSITY_260,
                DisplayUtil.getUiDensityForAutomotive(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_180));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testScaleUpDisplayMetricsForAutomotive() {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = 1.0f;
        displayMetrics.densityDpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.xdpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.ydpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.widthPixels = 100;
        displayMetrics.heightPixels = 100;

        DisplayUtil.scaleUpDisplayMetricsForAutomotive(
                ContextUtils.getApplicationContext(), displayMetrics);
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

    @Test
    @Config(
            sdk = Build.VERSION_CODES.R,
            shadows = {
                ShadowWindowMetrics.class,
            })
    public void testScaleUpConfigurationForAutomotiveAccountsForSystemBars() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            // Disable scaling. This test just checks if we are accounting for
                            // system bars.
                            DisplayUtil.setUiScalingFactorForAutomotiveForTesting(1f);
                            Configuration configuration = new Configuration();
                            DisplayUtil.scaleUpConfigurationForAutomotive(activity, configuration);

                            assertEquals(
                                    "Configuration.widthPixels should ignore system bar insets.",
                                    activity.getResources().getDisplayMetrics().widthPixels
                                            - TEST_LEFT_INSET
                                            - TEST_RIGHT_INSET,
                                    configuration.screenWidthDp);
                            assertEquals(
                                    "Configuration.heightPixels should ignore system bar insets.",
                                    activity.getResources().getDisplayMetrics().heightPixels
                                            - TEST_STATUS_BAR_HEIGHT
                                            - TEST_NAVIGATION_BAR_HEIGHT,
                                    configuration.screenHeightDp);
                        });
    }
}

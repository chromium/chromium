// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.display;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.WindowInsets;
import android.view.WindowMetrics;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.CommandLine;
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private DisplayAndroidManager mDisplayAndroidManager;

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
    public void testGetUiDensityForAutomotive_clampedToMaxValue() {
        assertEquals(
                "Density 200 should be scaled to 280 when the scaling factor is 1.34.",
                DisplayMetrics.DENSITY_280,
                DisplayUtil.getUiDensityForAutomotive(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_200));

        CommandLine.getInstance().appendSwitchWithValue("clamp-automotive-scale-up", "1.0");
        assertEquals(
                "The scaling factor should be clamped to the max scaling factor of 1.",
                DisplayMetrics.DENSITY_200,
                DisplayUtil.getUiDensityForAutomotive(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_200));

        CommandLine.getInstance().removeSwitch("clamp-automotive-scale-up");
    }

    // Tests when we have opted out of Clank's internal scaling when display compatibility is true.
    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testGetUiDensityForAutomotive_displayCompatTrue() {
        DisplayUtil.setCarmaPhase1Version2ComplianceForTesting(true);
        DisplayUtil.setIsDisplayCompatAppForTesting(true);

        assertEquals(
                "Density should be the base density when opted out of Clank's internal scaling.",
                DisplayMetrics.DENSITY_DEFAULT,
                DisplayUtil.getUiDensityForAutomotive(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_DEFAULT));
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

    // Unit Tests for Android XR based on Android 14.
    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testGetUiDensityForXr() {
        assertEquals(
                "Density 160 should be scaled to 174.0 and adjusted up to Density 190.",
                190,
                DisplayUtil.getUiDensityForXr(
                        ContextUtils.getApplicationContext(), DisplayMetrics.DENSITY_DEFAULT));
        assertEquals(
                "Density 250 should be scaled to 290.0 and adjusted up to Density 290",
                290,
                DisplayUtil.getUiDensityForXr(ContextUtils.getApplicationContext(), 250));
        assertEquals(
                "Density 210 should be scaled to 243.6 and adjusted up to Density 250.",
                250,
                DisplayUtil.getUiDensityForXr(ContextUtils.getApplicationContext(), 210));
        assertEquals(
                "Density 300 should be scaled to 348.0 and adjusted up to Density 350.",
                350,
                DisplayUtil.getUiDensityForXr(ContextUtils.getApplicationContext(), 300));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testScaleUpDisplayMetricsForXr() {
        final int densityDefaultScale = 190;
        final int widthInPixels = 100;
        final int heightInPixels = 100;
        final DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = 1.0f;
        displayMetrics.densityDpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.xdpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.ydpi = DisplayMetrics.DENSITY_DEFAULT;
        displayMetrics.widthPixels = widthInPixels;
        displayMetrics.heightPixels = heightInPixels;

        DisplayUtil.scaleUpDisplayMetricsForXr(
                ContextUtils.getApplicationContext(), displayMetrics);
        assertEquals(
                "The DisplayMetrics densityDpi should be scaled up by the XR scale-up factor.",
                densityDefaultScale,
                displayMetrics.densityDpi);
        assertEquals(
                "The DisplayMetrics xdpi should be scaled up by the XR scale-up factor.",
                densityDefaultScale,
                (int) displayMetrics.xdpi);
        assertEquals(
                "The DisplayMetrics ydpi should be scaled up by the XR scale-up factor.",
                densityDefaultScale,
                (int) displayMetrics.ydpi);
        assertEquals(
                "The DisplayMetrics widthPixels should not be affected by the XR scale-up factor.",
                widthInPixels,
                displayMetrics.widthPixels);
        assertEquals(
                "The DisplayMetrics heightPixels should not be affected by the XR scale-up factor.",
                heightInPixels,
                displayMetrics.heightPixels);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.TIRAMISU)
    public void testScaleUpConfigurationForXr() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            Configuration configuration = new Configuration();
                            DisplayUtil.setUiScalingFactorForXrForTesting(1f);
                            DisplayUtil.scaleUpConfigurationForXr(activity, configuration);

                            assertEquals(
                                    "Configuration.densityDpi should be 1.",
                                    activity.getResources().getDisplayMetrics().densityDpi,
                                    configuration.densityDpi);
                            assertEquals(
                                    "Configuration.widthPixels should be same with scale factor of"
                                            + " 1.",
                                    activity.getResources().getDisplayMetrics().widthPixels,
                                    configuration.screenWidthDp);
                            assertEquals(
                                    "Configuration.heightPixels should be same with scale factor of"
                                            + " 1.",
                                    activity.getResources().getDisplayMetrics().heightPixels,
                                    configuration.screenHeightDp);
                        });
    }

    @Test
    public void testDpToPx() {
        // Trivial test with density = 1, value = 0, expected = 0
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        assertEquals(
                "The value returned is incorrect (0)", 0, DisplayUtil.dpToPx(mDisplayAndroid, 0));

        // Trivial test with density = 1, value = 100, expected = 100
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        assertEquals(
                "The value returned is incorrect (1)",
                100,
                DisplayUtil.dpToPx(mDisplayAndroid, 100));

        // Rounding test with density = 1.375, value = 100, expected = 138
        when(mDisplayAndroid.getDipScale()).thenReturn(1.375f);
        assertEquals(
                "The value returned is incorrect (2)",
                138,
                DisplayUtil.dpToPx(mDisplayAndroid, 100));

        // Rounding test with density = 0.499, value = 100, expected = 50
        when(mDisplayAndroid.getDipScale()).thenReturn(0.499f);
        assertEquals(
                "The value returned is incorrect (3)",
                50,
                DisplayUtil.dpToPx(mDisplayAndroid, 100));

        // Rounding test with density = 0.4949, value = 100, expected = 49
        when(mDisplayAndroid.getDipScale()).thenReturn(0.4949f);
        assertEquals(
                "The value returned is incorrect (4)",
                49,
                DisplayUtil.dpToPx(mDisplayAndroid, 100));

        // Negative trivial test with density = 1, value = -100, expected = -100
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        assertEquals(
                "The value returned is incorrect (5)",
                -100,
                DisplayUtil.dpToPx(mDisplayAndroid, -100));

        // Negative rounding test with density = 1.375, value = -100, expected = -137 (tie-break
        // from -137.5 towards positive infinity)
        when(mDisplayAndroid.getDipScale()).thenReturn(1.375f);
        assertEquals(
                "The value returned is incorrect (6)",
                -137,
                DisplayUtil.dpToPx(mDisplayAndroid, -100));

        // Negative rounding test with density = 0.499, value = -100, expected = -50
        when(mDisplayAndroid.getDipScale()).thenReturn(0.499f);
        assertEquals(
                "The value returned is incorrect (7)",
                -50,
                DisplayUtil.dpToPx(mDisplayAndroid, -100));

        // Negative rounding test with density = 0.4949, value = -100, expected = -49
        when(mDisplayAndroid.getDipScale()).thenReturn(0.4949f);
        assertEquals(
                "The value returned is incorrect (8)",
                -49,
                DisplayUtil.dpToPx(mDisplayAndroid, -100));
    }

    @Test
    public void testPxToDp() {
        // Trivial test with density = 1, value = 0, expected = 0
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        assertEquals(
                "The value returned is incorrect (0)", 0, DisplayUtil.pxToDp(mDisplayAndroid, 0));

        // Trivial test with density = 1, value = 100, expected = 100
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        assertEquals(
                "The value returned is incorrect (1)",
                100,
                DisplayUtil.pxToDp(mDisplayAndroid, 100));

        // Rounding test with density = 1.6, value = 100, expected = 63
        when(mDisplayAndroid.getDipScale()).thenReturn(1.6f);
        assertEquals(
                "The value returned is incorrect (2)",
                63,
                DisplayUtil.pxToDp(mDisplayAndroid, 100));

        // Rounding test with density = 2.02, value = 100, expected = 50
        when(mDisplayAndroid.getDipScale()).thenReturn(2.02f);
        assertEquals(
                "The value returned is incorrect (3)",
                50,
                DisplayUtil.pxToDp(mDisplayAndroid, 100));

        // Rounding test with density = 2.0205, value = 100, expected = 49
        when(mDisplayAndroid.getDipScale()).thenReturn(2.0205f);
        assertEquals(
                "The value returned is incorrect (4)",
                49,
                DisplayUtil.pxToDp(mDisplayAndroid, 100));

        // Negative trivial test with density = 1, value = -100, expected = -100
        when(mDisplayAndroid.getDipScale()).thenReturn(1.0f);
        assertEquals(
                "The value returned is incorrect (5)",
                -100,
                DisplayUtil.pxToDp(mDisplayAndroid, -100));

        // Negative rounding test with density = 1.6, value = -100, expected = -62 (tie-break from
        // -62.5 towards positive infinity)
        when(mDisplayAndroid.getDipScale()).thenReturn(1.6f);
        assertEquals(
                "The value returned is incorrect (6)",
                -62,
                DisplayUtil.pxToDp(mDisplayAndroid, -100));

        // Negative rounding test with density = 2.02, value = -100, expected = -50
        when(mDisplayAndroid.getDipScale()).thenReturn(2.02f);
        assertEquals(
                "The value returned is incorrect (7)",
                -50,
                DisplayUtil.pxToDp(mDisplayAndroid, -100));

        // Negative rounding test with density = 2.0205, value = -100, expected = -49
        when(mDisplayAndroid.getDipScale()).thenReturn(2.0205f);
        assertEquals(
                "The value returned is incorrect (8)",
                -49,
                DisplayUtil.pxToDp(mDisplayAndroid, -100));
    }

    @Test
    public void testClampRect_inputRectInsideLimitingRect() {
        Rect limitingRect = new Rect(0, 0, 1920, 1080);
        Rect inputRect = new Rect(100, 200, 700, 800);
        assertEquals(
                "The inputRect should have been preserved as it's' already inside limitingRect",
                inputRect,
                DisplayUtil.clampRect(inputRect, limitingRect));
    }

    @Test
    public void testClampRect_inputRectPartiallyOutsideLimitingRectInHorizontalAxis() {
        Rect limitingRect = new Rect(0, 0, 1000, 1000);
        Rect inputRect = new Rect(-100, 200, 700, 800);

        // Size can be preserved, there is exactly one Rect with given size distant by 100 px in
        // Manhattan metric that fits inside the limitingRect, and there is no valid Rect with given
        // size closer than 100 px.
        Rect expectedRect = new Rect(0, 200, 800, 800);
        assertEquals(
                "The inputRect is partially outside the limitingRect",
                expectedRect,
                DisplayUtil.clampRect(inputRect, limitingRect));
    }

    @Test
    public void testClampRect_inputRectPartiallyOutsideLimitingRectInVerticalAxis() {
        Rect limitingRect = new Rect(0, 0, 1000, 1000);
        Rect inputRect = new Rect(200, -100, 800, 700);

        // Size can be preserved, there is exactly one Rect with given size distant by 100 px in
        // Manhattan metric that fits inside the limitingRect, and there is no valid Rect with given
        // size closer than 100 px.
        Rect expectedRect = new Rect(200, 0, 800, 800);
        assertEquals(
                "The inputRect is partially outside the limitingRect",
                expectedRect,
                DisplayUtil.clampRect(inputRect, limitingRect));
    }

    @Test
    public void testClampRect_inputRectFullyOutsideLimitingRect() {
        Rect limitingRect = new Rect(0, 0, 1000, 1000);
        Rect inputRect = new Rect(1100, 1200, 1600, 1800);

        // Size can be preserved, there is exactly one Rect with given size distant by 1400 px in
        // Manhattan metric that fits inside the limitingRect, and there is no valid Rect with given
        // size closer than 1400 px.
        Rect expectedRect = new Rect(500, 400, 1000, 1000);
        assertEquals(
                "The inputRect is fully outside the limitingRect",
                expectedRect,
                DisplayUtil.clampRect(inputRect, limitingRect));
    }

    @Test
    public void testClampRect_inputRectFullyOutsideAndWiderThanLimitingRect() {
        Rect limitingRect = new Rect(0, 0, 1100, 1200);
        Rect inputRect = new Rect(-100, 1400, 1200, 1800);

        // Size cannot be preserved in horizontal axis. The least displacement in the vertical axis
        // to get a Rect inside the display is 400px.
        Rect expectedRect = new Rect(0, 800, 1100, 1200);
        assertEquals(
                "The inputRect is fully outside and wider than the limitingRect",
                expectedRect,
                DisplayUtil.clampRect(inputRect, limitingRect));
    }

    @Test
    public void testClampRect_inputRectBiggerThanLimitingRect() {
        Rect limitingRect = new Rect(0, 0, 1400, 1200);
        Rect inputRect = new Rect(-100, 1400, 1400, 3000);
        assertEquals(
                "The inputRect is bigger in both dimensions than the limitingRect",
                limitingRect,
                DisplayUtil.clampRect(inputRect, limitingRect));
    }

    @Test
    public void testClampWindowToDisplay() {
        when(mDisplayAndroid.getLocalBounds()).thenReturn(new Rect(0, 0, 1920, 1080));
        final Rect testBounds = new Rect(100, 200, 700, 800);
        assertEquals(
                "The bounds should have been preserved as they were already inside display",
                testBounds,
                DisplayUtil.clampWindowToDisplay(testBounds, mDisplayAndroid));
    }

    @Test
    public void scaleToEnclosingRect() {
        Rect rect = new Rect(-10, -20, 10, 20);

        // An empty rect.
        assertEquals(
                "Scaling an empty rect.",
                new Rect(0, 0, 0, 0),
                DisplayUtil.scaleToEnclosingRect(new Rect(0, 0, 0, 0), 1.5f));

        // Scale = 0.
        assertEquals(
                "Scaling by 0.0 should result in an empty rect at the origin.",
                new Rect(0, 0, 0, 0),
                DisplayUtil.scaleToEnclosingRect(rect, 0.0f));

        // Scale = 1.0, should be a no-op.
        assertEquals(
                "Scaling by 1.0 should not change the rect.",
                rect,
                DisplayUtil.scaleToEnclosingRect(rect, 1.0f));

        // Scaling Up.
        assertEquals(
                "Scaling a rect with negative coordinates.",
                new Rect(-15, -30, 15, 30),
                DisplayUtil.scaleToEnclosingRect(rect, 1.5f));

        // Scaling down.
        assertEquals(
                "Scaling down by 0.5 should halve the coordinates.",
                new Rect(-5, -10, 5, 10),
                DisplayUtil.scaleToEnclosingRect(rect, 0.5f));

        // Fractional results.
        // left = -10 * 1.33 = -13.3, top = -20 * 1.33 = -26.6
        // right = 10 * 1.33 = 13.3, bottom = 20 * 1.33 = 26.6
        assertEquals(
                "Scaling a rect with fractional results.",
                new Rect(-14, -27, 14, 27),
                DisplayUtil.scaleToEnclosingRect(rect, 1.33f));

        // Fractional results.
        // left = -10 * 1.05 = -10.5, top = -20 * 1.05 = -21
        // right = 10 * 1.05 = 10.5, bottom = 20 * 1.05 = 21
        assertEquals(
                "Scaling a rect with 0.5 fractional results.",
                new Rect(-11, -21, 11, 21),
                DisplayUtil.scaleToEnclosingRect(rect, 1.05f));
    }

    private void prepareDisplayAndroid(Rect bounds, Rect localBounds, float dipScale) {
        when(mDisplayAndroid.getBounds()).thenReturn(bounds);
        when(mDisplayAndroid.getLocalBounds()).thenReturn(localBounds);
        when(mDisplayAndroid.getDipScale()).thenReturn(dipScale);
    }

    @Test
    public void testConvertGlobalDipToLocalPxCoordinates() {
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);

        // No matching display
        {
            Rect globalCoordinatesDip = new Rect(100, 200, 300, 400);
            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip)).thenReturn(null);

            assertNull(
                    "Conversion should return null when no display matches: ",
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Empty coordinates
        {
            prepareDisplayAndroid(new Rect(0, 0, 1536, 864), new Rect(0, 0, 1920, 1080), 1.25f);
            Rect globalCoordinatesDip = new Rect(0, 0, 0, 0);
            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "Conversion of an empty DIP rect should result in an empty px rect: ",
                    Pair.create(mDisplayAndroid, new Rect(0, 0, 0, 0)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Coordinates match display bounds
        {
            prepareDisplayAndroid(new Rect(0, 0, 1027, 578), new Rect(0, 0, 1920, 1080), 1.87f);
            Rect globalCoordinatesDip = new Rect(0, 0, 1027, 578);

            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "DIP coordinates matching display bounds should convert to display local px"
                            + " bounds:",
                    Pair.create(mDisplayAndroid, new Rect(0, 0, 1920, 1080)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Coordinates almost match display bounds
        {
            prepareDisplayAndroid(new Rect(0, 0, 1536, 864), new Rect(0, 0, 1920, 1080), 1.25f);
            Rect globalCoordinatesDip = new Rect(1, 1, 1535, 863);
            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "Conversion for coordinates close to the display bounds failed: ",
                    Pair.create(mDisplayAndroid, new Rect(1, 1, 1919, 1079)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Scale factor 1.5
        {
            prepareDisplayAndroid(new Rect(-640, -360, 640, 360), new Rect(0, 0, 1920, 1080), 1.5f);
            Rect globalCoordinatesDip = new Rect(-505, -307, 621, 353);
            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "Conversion for scale factor 1.5 failed: ",
                    Pair.create(mDisplayAndroid, new Rect(202, 79, 1892, 1070)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Coordinates fully inside display
        {
            prepareDisplayAndroid(
                    new Rect(-70, -179, 1105, 576), new Rect(0, 0, 1983, 1275), 1.69f);
            Rect globalCoordinatesDip = new Rect(-57, -124, 354, 489);

            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "Conversion for coordinates fully inside the display failed: ",
                    Pair.create(mDisplayAndroid, new Rect(21, 92, 714, 1128)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Coordinates fully cover display
        {
            prepareDisplayAndroid(
                    new Rect(170, -1411, 2705, 438), new Rect(0, 0, 1983, 1275), 0.69f);
            Rect globalCoordinatesDip = new Rect(111, -1574, 2899, 442);

            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "Conversion for coordinates fully covering the display failed: ",
                    Pair.create(mDisplayAndroid, new Rect(-41, -113, 2117, 1278)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Coordinates intersect display
        {
            prepareDisplayAndroid(
                    new Rect(-960, -720, 1536, 864), new Rect(0, 0, 3120, 1980), 1.25f);
            Rect globalCoordinatesDip = new Rect(-1280, -480, 1760, 320);

            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "Conversion for coordinates partially intersecting the display failed: ",
                    Pair.create(mDisplayAndroid, new Rect(-400, 300, 3400, 1300)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
        // Coordinates are between display's px and dp coordinates
        {
            prepareDisplayAndroid(
                    new Rect(-960, -720, 1536, 864), new Rect(0, 0, 3120, 1980), 1.25f);
            Rect globalCoordinatesDip = new Rect(-1107, -797, 1772, 999);

            when(mDisplayAndroidManager.getDisplayMatching(globalCoordinatesDip))
                    .thenReturn(mDisplayAndroid);

            assertEquals(
                    "Conversion for coordinates between display's px and dp coordinates failed: ",
                    Pair.create(mDisplayAndroid, new Rect(-184, -97, 3415, 2149)),
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(globalCoordinatesDip));
        }
    }

    @Test
    public void testConvertLocalPxToGlobalDipCoordinates_fullDisplayBounds() {
        final float displayDipScale = 1.25f;
        final Rect displayLocalCoordinatesPx = new Rect(0, 0, 3120, 1980);
        final Rect displayGlobalCoordinatesDp = new Rect(-960, -720, 1536, 864);

        prepareDisplayAndroid(
                displayGlobalCoordinatesDp, displayLocalCoordinatesPx, displayDipScale);
        assertEquals(
                "Conversion between coordinate systems failed",
                displayGlobalCoordinatesDp,
                DisplayUtil.convertLocalPxToGlobalDipCoordinates(
                        mDisplayAndroid, displayLocalCoordinatesPx));
    }

    @Test
    public void testConvertLocalPxToGlobalDipCoordinates_rounding() {
        final float displayDipScale = 1.2f;
        final Rect displayLocalCoordinatesPx = new Rect(0, 0, 1000, 1000);
        final Rect displayGlobalCoordinatesDp = new Rect(0, 0, 834, 834);
        final Rect testLocalCoordinatesPx = new Rect(1, 3, 100, 1000);
        final Rect expectedGlobalCoordinatesDp = new Rect(0, 2, 84, 834);

        prepareDisplayAndroid(
                displayGlobalCoordinatesDp, displayLocalCoordinatesPx, displayDipScale);
        assertEquals(
                "Conversion between coordinate systems failed",
                expectedGlobalCoordinatesDp,
                DisplayUtil.convertLocalPxToGlobalDipCoordinates(
                        mDisplayAndroid, testLocalCoordinatesPx));
    }

    @Test
    public void testConvertLocalPxToGlobalDipCoordinates_emptyRect() {
        final float displayDipScale = 1.3f;
        final Rect displayLocalCoordinatesPx = new Rect(0, 0, 1000, 1000);
        final Rect displayGlobalCoordinatesDp = new Rect(0, 0, 770, 770);
        final Rect testLocalCoordinatesPx = new Rect(123, 456, 123, 456);
        final Rect expectedGlobalCoordinatesDp = new Rect(94, 350, 95, 351);

        prepareDisplayAndroid(
                displayGlobalCoordinatesDp, displayLocalCoordinatesPx, displayDipScale);
        assertEquals(
                "Conversion between coordinate systems failed",
                expectedGlobalCoordinatesDp,
                DisplayUtil.convertLocalPxToGlobalDipCoordinates(
                        mDisplayAndroid, testLocalCoordinatesPx));
    }

    @Test
    public void testConvertLocalPxToGlobalDipCoordinates_displayOriginTranslation() {
        final float displayDipScale = 1.75f;
        final Rect displayLocalCoordinatesPx = new Rect(0, 0, 1000, 1000);
        final Rect displayGlobalCoordinatesDp = new Rect(-100, -200, 472, 372);
        final Rect testLocalCoordinatesPx = new Rect(12, 34, 567, 890);
        final Rect expectedGlobalCoordinatesDp =
                new Rect(-100 + 6, -200 + 19, -100 + 324, -200 + 509);

        prepareDisplayAndroid(
                displayGlobalCoordinatesDp, displayLocalCoordinatesPx, displayDipScale);
        assertEquals(
                "Conversion between coordinate systems failed",
                expectedGlobalCoordinatesDp,
                DisplayUtil.convertLocalPxToGlobalDipCoordinates(
                        mDisplayAndroid, testLocalCoordinatesPx));
    }

    @Test
    public void testConvertLocalPxToGlobalDipCoordinates_coordinatesNotInsideDisplay() {
        final float displayDipScale = 0.5f;
        final Rect displayLocalCoordinatesPx = new Rect(0, 0, 1000, 1000);
        final Rect displayGlobalCoordinatesDp = new Rect(-500, 700, 1500, 2700);
        final Rect testLocalCoordinatesPx = new Rect(-600, -400, -100, 1300);
        final Rect expectedGlobalCoordinatesDp =
                new Rect(-500 - 1200, 700 - 800, -500 - 200, 700 + 2600);

        prepareDisplayAndroid(
                displayGlobalCoordinatesDp, displayLocalCoordinatesPx, displayDipScale);
        assertEquals(
                "Conversion between coordinate systems failed",
                expectedGlobalCoordinatesDp,
                DisplayUtil.convertLocalPxToGlobalDipCoordinates(
                        mDisplayAndroid, testLocalCoordinatesPx));
    }
}

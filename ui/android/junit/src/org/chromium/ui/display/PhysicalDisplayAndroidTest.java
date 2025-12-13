// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.hardware.display.DeviceProductInfo;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Surface;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;

@RunWith(BaseRobolectricTestRunner.class)
public class PhysicalDisplayAndroidTest {
    // Test values
    private static final int TEST_DISPLAY_ID = 3;
    private static final String TEST_DISPLAY_NAME = "TEST DISPLAY NAME";

    private static final Rect TEST_DISPLAY_PIXEL_BOUNDS = new Rect(0, 0, 1920, 1080);
    private static final Insets TEST_DISPLAY_INSETS = Insets.of(10, 20, 30, 40);
    private static final RectF TEST_DISPLAY_DIP_ABSOLUTE_COORDINATES =
            new RectF(100f, 200f, 1636f, 1064f);

    private static final int TEST_DISPLAY_ROTATION = Surface.ROTATION_180;
    private static final int TEST_DISPLAY_ROTATION_DEGREES = 180;

    private static final float TEST_DISPLAY_DIP_SCALE = 1.25f;
    private static final int TEST_DISPLAY_DENSITY_DPI = DisplayMetrics.DENSITY_HIGH;
    private static final float TEST_DISPLAY_XDPI = 1.15f;
    private static final float TEST_DISPLAY_YDPI = 1.35f;

    private static final float TEST_FORCERD_DIP_SCALE = 2.5f;
    private static final float TEST_AUTOMOTIVE_UI_SCALING_FACTOR = 4f;
    private static final float TEST_XR_UI_SCALING_FACTOR = 0.5f;

    // TEST_DISPLAY_PIXEL_BOUNDS = {0, 0, 1920, 1080}, TEST_DISPLAY_DIP_SCALE = 1.25.
    private static final Rect TEST_DISPLAY_DIP_BOUNDS = new Rect(0, 0, 1536, 864);
    // TEST_DISPLAY_INSETS = {10, 20, 30, 40}, TEST_DISPLAY_DIP_SCALE = 1.25.
    private static final Rect TEST_DISPLAY_DIP_WORK_AREA = new Rect(8, 16, 1512, 832);

    private static final float TEST_DISPLAY_REFRESH_RATE = 15.45f;
    private static final Display.Mode TEST_DISPLAY_MODE_CURRENT =
            new Display.Mode(
                    1,
                    TEST_DISPLAY_PIXEL_BOUNDS.width(),
                    TEST_DISPLAY_PIXEL_BOUNDS.height(),
                    60.0f);
    private static final Display.Mode TEST_DISPLAY_MODE =
            new Display.Mode(
                    2,
                    TEST_DISPLAY_PIXEL_BOUNDS.width(),
                    TEST_DISPLAY_PIXEL_BOUNDS.height(),
                    90.0f);
    private static final Display.Mode[] TEST_DISPLAY_SUPPORTED_MODES =
            new Display.Mode[] {TEST_DISPLAY_MODE_CURRENT, TEST_DISPLAY_MODE};

    // Physical display default values
    private static final int DEFAULT_BITS_PER_PIXEL = 24;
    private static final int DEFAULT_BITS_PER_COMPONENT = 8;

    private static final DisplayAndroid.AdaptiveRefreshRateInfo DEFAULT_ADAPTIVE_REFRESH_RATE_INFO =
            new DisplayAndroid.AdaptiveRefreshRateInfo(false, 0.0f);

    private static final boolean DEFAULT_IS_WIDE_COLOR_GAMUT = false;
    private static final boolean DEFAULT_DISPLAY_IS_HDR = false;
    private static final float DEFAULT_DISPLAY_HDR_SDR_RATIO = 1.0f;

    private static final boolean DEFAULT_DISPLAY_IS_INTERNAL = false;

    // Test constants
    private static final float DELTA = 0.000001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Mocks
    @Mock private Display mDisplay;
    @Mock private DisplayAndroidManager mDisplayAndroidManager;
    @Spy Context mAppContext = ContextUtils.getApplicationContext();
    @Mock private Context mWindowContext;
    @Mock private WindowManager mWindowManager;
    @Mock private Resources mResources;
    @Mock private WindowMetrics mCurrentWindowMetrics;
    @Mock private WindowMetrics mMaximumWindowMetrics;
    @Mock private WindowInsets mWindowInsets;

    @Before
    public void setup() {
        DeviceInfo.setIsAutomotiveForTesting(false);
        DeviceInfo.setIsXrForTesting(false);

        // This is necessary because, when DisplayAndroidManager is created, another default
        // PhysicalDisplayAndroid is constructed, this produces unwanted logs.
        DisplayAndroidManager.setInstanceForTesting(mDisplayAndroidManager);
        doNothing().when(mDisplayAndroidManager).updateDisplayOnNativeSide(any());

        doReturn(TEST_DISPLAY_ID).when(mDisplay).getDisplayId();
        doReturn(TEST_DISPLAY_NAME).when(mDisplay).getName();
        doReturn(TEST_DISPLAY_ROTATION).when(mDisplay).getRotation();
        doReturn(TEST_DISPLAY_REFRESH_RATE).when(mDisplay).getRefreshRate();
        doReturn(TEST_DISPLAY_MODE_CURRENT).when(mDisplay).getMode();
        doReturn(TEST_DISPLAY_SUPPORTED_MODES).when(mDisplay).getSupportedModes();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            setupMocksForSdkS();
        } else {
            setupMocksForPreSdkS();
        }
    }

    /** Helper method to configure mocks for Android S and newer. */
    private void setupMocksForSdkS() {
        ContextUtils.initApplicationContextForTests(mAppContext);
        doReturn(mWindowContext)
                .when(mAppContext)
                .createWindowContext(
                        eq(mDisplay), eq(WindowManager.LayoutParams.TYPE_APPLICATION), eq(null));
        doReturn(mResources).when(mWindowContext).getResources();
        doReturn(mDisplay).when(mWindowContext).getDisplay();
        doReturn(mWindowManager).when(mWindowContext).getSystemService(eq(WindowManager.class));

        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.density = TEST_DISPLAY_DIP_SCALE;
        displayMetrics.densityDpi = TEST_DISPLAY_DENSITY_DPI;
        displayMetrics.xdpi = TEST_DISPLAY_XDPI;
        displayMetrics.ydpi = TEST_DISPLAY_YDPI;
        doReturn(displayMetrics).when(mResources).getDisplayMetrics();

        doReturn(mCurrentWindowMetrics).when(mWindowManager).getCurrentWindowMetrics();
        doReturn(mWindowInsets).when(mCurrentWindowMetrics).getWindowInsets();
        doReturn(TEST_DISPLAY_INSETS)
                .when(mWindowInsets)
                .getInsetsIgnoringVisibility(
                        eq(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout()));

        doReturn(mMaximumWindowMetrics).when(mWindowManager).getMaximumWindowMetrics();
        doReturn(TEST_DISPLAY_PIXEL_BOUNDS).when(mMaximumWindowMetrics).getBounds();
    }

    /** Helper method to configure mocks for Android versions older than S. */
    private void setupMocksForPreSdkS() {
        doAnswer(
                        invocation -> {
                            Point outSize = invocation.getArgument(0);
                            outSize.x = TEST_DISPLAY_PIXEL_BOUNDS.width();
                            outSize.y = TEST_DISPLAY_PIXEL_BOUNDS.height();

                            return null;
                        })
                .when(mDisplay)
                .getRealSize(any(Point.class));
        doAnswer(
                        invocation -> {
                            DisplayMetrics outMetrics = invocation.getArgument(0);
                            outMetrics.density = TEST_DISPLAY_DIP_SCALE;
                            outMetrics.densityDpi = TEST_DISPLAY_DENSITY_DPI;
                            outMetrics.xdpi = TEST_DISPLAY_XDPI;
                            outMetrics.ydpi = TEST_DISPLAY_YDPI;

                            return null;
                        })
                .when(mDisplay)
                .getRealMetrics(any(DisplayMetrics.class));
    }

    @After
    public void teardown() {
        CommandLine.getInstance().removeSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED);
        CommandLine.getInstance().removeSwitch(DisplaySwitches.XR_WEB_UI_SCALE_UP_ENABLED);
    }

    private void setupAutomotive() {
        DeviceInfo.setIsAutomotiveForTesting(true);
        DisplayUtil.setCarmaPhase1Version2ComplianceForTesting(false);
        DisplayUtil.setUiScalingFactorForAutomotiveForTesting(TEST_AUTOMOTIVE_UI_SCALING_FACTOR);
        CommandLine.getInstance().appendSwitch(DisplaySwitches.AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED);
    }

    private void setupXr() {
        DeviceInfo.setIsXrForTesting(true);
        DisplayUtil.setUiScalingFactorForXrForTesting(TEST_XR_UI_SCALING_FACTOR);
        CommandLine.getInstance().appendSwitch(DisplaySwitches.XR_WEB_UI_SCALE_UP_ENABLED);
    }

    @Test
    public void testPhysicalDisplayAndroidGeneralUpdateFromDisplay() {
        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        checkDisplayGeneral(physicalDisplayAndroid);
        // Insets are not taken into account, so dipGlobalBounds and dipWorkArea are the same.
        checkDisplaySize(
                physicalDisplayAndroid,
                TEST_DISPLAY_PIXEL_BOUNDS,
                TEST_DISPLAY_DIP_BOUNDS,
                TEST_DISPLAY_DIP_BOUNDS);
        checkDisplayIsInternal(physicalDisplayAndroid, DEFAULT_DISPLAY_IS_INTERNAL);
    }

    @Test
    public void testPhysicalDisplayAndroidUpdateFromDisplayWithForcedDIPScale() {
        PhysicalDisplayAndroid.setHasForcedDIPScaleForTesting(TEST_FORCERD_DIP_SCALE);

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        // The TEST_FORCERD_DIP_SCALE is two times bigger than the TEST_DISPLAY_DIP_SCALE, so the
        // coordinate should be two times smaller.
        final Rect dipGlobalBounds = new Rect(0, 0, 768, 432);

        checkDisplaySize(
                physicalDisplayAndroid,
                TEST_DISPLAY_PIXEL_BOUNDS,
                dipGlobalBounds,
                dipGlobalBounds);
    }

    @Test
    public void testPhysicalDisplayAndroidUpdateFromDisplayForAutomotive() {
        setupAutomotive();

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        // The TEST_AUTOMOTIVE_UI_SCALING_FACTOR is 4, so the coordinate should be 4 times
        // smaller.
        final Rect dipGlobalBounds = new Rect(0, 0, 384, 216);

        checkDisplaySize(
                physicalDisplayAndroid,
                TEST_DISPLAY_PIXEL_BOUNDS,
                dipGlobalBounds,
                dipGlobalBounds);
    }

    @Test
    public void testPhysicalDisplayAndroidUpdateFromDisplayForXr() {
        setupXr();

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        // The TEST_XR_UI_SCALING_FACTOR is 0.5, so the coordinate should be 2 times
        // bigger.
        final Rect dipGlobalBounds = new Rect(0, 0, 3072, 1728);

        checkDisplaySize(
                physicalDisplayAndroid,
                TEST_DISPLAY_PIXEL_BOUNDS,
                dipGlobalBounds,
                dipGlobalBounds);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidGeneralUpdateFromConfiguration() {
        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        checkDisplayGeneral(physicalDisplayAndroid);
        checkDisplaySize(
                physicalDisplayAndroid,
                TEST_DISPLAY_PIXEL_BOUNDS,
                TEST_DISPLAY_DIP_BOUNDS,
                TEST_DISPLAY_DIP_WORK_AREA);
        checkDisplayIsInternal(physicalDisplayAndroid, DEFAULT_DISPLAY_IS_INTERNAL);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidUpdateFromConfigurationWithForcedDIPScale() {
        PhysicalDisplayAndroid.setHasForcedDIPScaleForTesting(TEST_FORCERD_DIP_SCALE);

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        // The TEST_FORCERD_DIP_SCALE is two times bigger than the TEST_DISPLAY_DIP_SCALE, so the
        // coordinate should be two times smaller.
        final Rect dipGlobalBounds = new Rect(0, 0, 768, 432);
        final Rect dipWorkArea = new Rect(4, 8, 756, 416);

        checkDisplaySize(
                physicalDisplayAndroid, TEST_DISPLAY_PIXEL_BOUNDS, dipGlobalBounds, dipWorkArea);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidlUpdateFromConfigurationForAutomotive() {
        setupAutomotive();

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        // The TEST_AUTOMOTIVE_UI_SCALING_FACTOR is 4, so the coordinate should be 4 times
        // smaller.
        final Rect dipGlobalBounds = new Rect(0, 0, 384, 216);
        final Rect dipWorkArea = new Rect(2, 4, 378, 208);

        checkDisplaySize(
                physicalDisplayAndroid, TEST_DISPLAY_PIXEL_BOUNDS, dipGlobalBounds, dipWorkArea);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidlUpdateFromConfigurationForXr() {
        setupXr();

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, null, false);

        // The TEST_XR_UI_SCALING_FACTOR is 0.5, so the coordinate should be 2 times
        // bigger.
        final Rect dipGlobalBounds = new Rect(0, 0, 3072, 1728);
        final Rect dipWorkArea = new Rect(16, 32, 3024, 1664);

        checkDisplaySize(
                physicalDisplayAndroid, TEST_DISPLAY_PIXEL_BOUNDS, dipGlobalBounds, dipWorkArea);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidWithAbsoluteCoordinates() {
        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, TEST_DISPLAY_DIP_ABSOLUTE_COORDINATES, false);

        // TEST_DISPLAY_DIP_ABSOLUTE_COORDINATES = {0, 0, 1636, 1064}.
        final Rect dipGlobalBounds = new Rect(100, 200, 1636, 1064);
        // TEST_DISPLAY_DIP_ABSOLUTE_COORDINATES = {0, 0, 1636, 1064},
        // TEST_DISPLAY_INSETS = {10, 20, 30, 40}, TEST_DISPLAY_DIP_SCALE = 1.25.
        final Rect dipWorkArea = new Rect(108, 216, 1612, 1032);
        checkDisplaySize(
                physicalDisplayAndroid, TEST_DISPLAY_PIXEL_BOUNDS, dipGlobalBounds, dipWorkArea);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidWithAbsoluteCoordinatesAndForcedDIPScale() {
        PhysicalDisplayAndroid.setHasForcedDIPScaleForTesting(TEST_FORCERD_DIP_SCALE);

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, TEST_DISPLAY_DIP_ABSOLUTE_COORDINATES, false);

        // The TEST_FORCERD_DIP_SCALE is two times bigger than the TEST_DISPLAY_DIP_SCALE, so the
        // coordinate should be two times smaller.
        final Rect dipGlobalBounds = new Rect(50, 100, 818, 532);
        final Rect dipWorkArea = new Rect(54, 108, 806, 516);

        checkDisplaySize(
                physicalDisplayAndroid, TEST_DISPLAY_PIXEL_BOUNDS, dipGlobalBounds, dipWorkArea);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidlWithAbsoluteCoordinatesForAutomotive() {
        setupAutomotive();

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, TEST_DISPLAY_DIP_ABSOLUTE_COORDINATES, false);

        // The TEST_AUTOMOTIVE_UI_SCALING_FACTOR is 4, so the coordinate should be 4 times
        // smaller.
        final Rect dipGlobalBounds = new Rect(25, 50, 409, 266);
        final Rect dipWorkArea = new Rect(27, 54, 403, 258);

        checkDisplaySize(
                physicalDisplayAndroid, TEST_DISPLAY_PIXEL_BOUNDS, dipGlobalBounds, dipWorkArea);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidlWithAbsoluteCoordinatesForXr() {
        setupXr();

        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, TEST_DISPLAY_DIP_ABSOLUTE_COORDINATES, false);

        // The TEST_XR_UI_SCALING_FACTOR is 0.5, so the coordinate should be 2 times
        // bigger.
        final Rect dipGlobalBounds = new Rect(200, 400, 3272, 2128);
        final Rect dipWorkArea = new Rect(216, 432, 3224, 2064);

        checkDisplaySize(
                physicalDisplayAndroid, TEST_DISPLAY_PIXEL_BOUNDS, dipGlobalBounds, dipWorkArea);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void testPhysicalDisplayAndroidIsInternal() {
        final DeviceProductInfo deviceProductInfo = mock(DeviceProductInfo.class);
        doReturn(deviceProductInfo).when(mDisplay).getDeviceProductInfo();

        {
            doReturn(DeviceProductInfo.CONNECTION_TO_SINK_UNKNOWN)
                    .when(deviceProductInfo)
                    .getConnectionToSinkType();
            final PhysicalDisplayAndroid physicalDisplayAndroid =
                    new PhysicalDisplayAndroid(mDisplay, null, false);
            checkDisplayIsInternal(physicalDisplayAndroid, false);
        }
        {
            doReturn(DeviceProductInfo.CONNECTION_TO_SINK_BUILT_IN)
                    .when(deviceProductInfo)
                    .getConnectionToSinkType();
            final PhysicalDisplayAndroid physicalDisplayAndroid =
                    new PhysicalDisplayAndroid(mDisplay, null, false);
            checkDisplayIsInternal(physicalDisplayAndroid, true);
        }
    }

    private void checkDisplaySize(
            PhysicalDisplayAndroid physicalDisplayAndroid,
            Rect pixelLocalBounds,
            Rect dipGlobalBounds,
            Rect dipWorkArea) {
        assertEquals(
                "Display height is incorrect: ",
                pixelLocalBounds.height(),
                physicalDisplayAndroid.getDisplayHeight());
        assertEquals(
                "Display width is incorrect: ",
                pixelLocalBounds.width(),
                physicalDisplayAndroid.getDisplayWidth());
        assertEquals(
                "Display bounds is incorrect: ",
                dipGlobalBounds,
                physicalDisplayAndroid.getBounds());
        assertArrayEquals(
                "Display bounds as array is incorrect: ",
                new int[] {
                    dipGlobalBounds.left,
                    dipGlobalBounds.top,
                    dipGlobalBounds.right,
                    dipGlobalBounds.bottom
                },
                physicalDisplayAndroid.getBoundsAsArray());
        assertEquals(
                "Display localBounds is incorrect: ",
                pixelLocalBounds,
                physicalDisplayAndroid.getLocalBounds());

        assertEquals(
                "Display workArea is incorrect: ",
                dipWorkArea,
                physicalDisplayAndroid.getWorkArea());
        assertArrayEquals(
                "Display workArea as array is incorrect: ",
                new int[] {
                    dipWorkArea.left, dipWorkArea.top, dipWorkArea.right, dipWorkArea.bottom
                },
                physicalDisplayAndroid.getWorkAreaAsArray());
    }

    public void checkDisplayIsInternal(
            PhysicalDisplayAndroid physicalDisplayAndroid, boolean isInternal) {
        assertEquals(
                "Display isInternal is incorrect: ",
                isInternal,
                physicalDisplayAndroid.isInternal());
    }

    private void checkDisplayGeneral(PhysicalDisplayAndroid physicalDisplayAndroid) {
        assertEquals(
                "Display id is incorrect: ",
                TEST_DISPLAY_ID,
                physicalDisplayAndroid.getDisplayId());
        assertEquals(
                "Display name is incorrect: ",
                TEST_DISPLAY_NAME,
                physicalDisplayAndroid.getDisplayName());

        assertEquals(
                "Display rotation is incorrect: ",
                TEST_DISPLAY_ROTATION,
                physicalDisplayAndroid.getRotation());
        assertEquals(
                "Display rotationDegrees is incorrect: ",
                TEST_DISPLAY_ROTATION_DEGREES,
                physicalDisplayAndroid.getRotationDegrees());

        assertEquals(
                "Display dipScale is incorrect: ",
                TEST_DISPLAY_DIP_SCALE,
                physicalDisplayAndroid.getDipScale(),
                DELTA);
        assertEquals(
                "Display xdpi is incorrect: ",
                TEST_DISPLAY_XDPI,
                physicalDisplayAndroid.getXdpi(),
                DELTA);
        assertEquals(
                "Display ydpi is incorrect: ",
                TEST_DISPLAY_YDPI,
                physicalDisplayAndroid.getYdpi(),
                DELTA);

        assertEquals(
                "Display bitsPerPixel is incorrect: ",
                DEFAULT_BITS_PER_PIXEL,
                physicalDisplayAndroid.getBitsPerPixel());
        assertEquals(
                "Display bitsPerComponent is incorrect: ",
                DEFAULT_BITS_PER_COMPONENT,
                physicalDisplayAndroid.getBitsPerComponent());
        assertEquals(
                "Display isWideColorGamut is incorrect: ",
                DEFAULT_IS_WIDE_COLOR_GAMUT,
                physicalDisplayAndroid.getIsWideColorGamut());

        assertEquals(
                "Display refreshRate is incorrect: ",
                TEST_DISPLAY_REFRESH_RATE,
                physicalDisplayAndroid.getRefreshRate(),
                DELTA);
        assertEquals(
                "Display supportedModes are inctorrect: ",
                Arrays.asList(TEST_DISPLAY_SUPPORTED_MODES),
                physicalDisplayAndroid.getSupportedModes());
        assertEquals(
                "Display currentMode is incorrect: ",
                TEST_DISPLAY_MODE_CURRENT,
                physicalDisplayAndroid.getCurrentMode());
        assertEquals(
                "Display adaptiveRefreshRateInfo: ",
                DEFAULT_ADAPTIVE_REFRESH_RATE_INFO,
                physicalDisplayAndroid.getAdaptiveRefreshRateInfo());

        assertEquals(
                "Display isHdr is incorrect: ",
                DEFAULT_DISPLAY_IS_HDR,
                physicalDisplayAndroid.getIsHdr());
        assertEquals(
                "Display HdrMaxLuminanceRatio is incorrect: ",
                DEFAULT_DISPLAY_HDR_SDR_RATIO,
                physicalDisplayAndroid.getHdrMaxLuminanceRatio(),
                DELTA);
    }
}

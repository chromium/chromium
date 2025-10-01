// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
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

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

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

    private static final int TEST_DISPLAY_ROTATION = Surface.ROTATION_180;
    private static final int TEST_DISPLAY_ROTATION_DEGREES = 180;

    private static final float TEST_DISPLAY_DIP_SCALE = 1.25f;
    private static final float TEST_DISPLAY_XDPI = 1.15f;
    private static final float TEST_DISPLAY_YDPI = 1.35f;

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
                            outMetrics.xdpi = TEST_DISPLAY_XDPI;
                            outMetrics.ydpi = TEST_DISPLAY_YDPI;

                            return null;
                        })
                .when(mDisplay)
                .getRealMetrics(any(DisplayMetrics.class));
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
    public void testPhysicalDisplayAndroidWithAbsoluteCoordinates() {
        final RectF displayDipAbsoluteCoordinates = new RectF(100f, 200f, 1636f, 1064f);
        final PhysicalDisplayAndroid physicalDisplayAndroid =
                new PhysicalDisplayAndroid(mDisplay, displayDipAbsoluteCoordinates, false);

        // displayDipAbsoluteCoordinates = {0, 0, 1636, 1064}.
        final Rect dipGlobalBounds = new Rect(100, 200, 1636, 1064);
        // displayDipAbsoluteCoordinates = {0, 0, 1636, 1064}, insets = {10, 20, 30, 40}, dipScale =
        // 1.25.
        final Rect dipWorkArea = new Rect(108, 216, 1612, 1032);
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

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.WindowManager;
import android.view.WindowMetrics;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroidTest.ShadowDisplayAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.lang.ref.WeakReference;

@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowDisplayAndroid.class, sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class WindowAndroidTest {

    @Mock private final Context mContext = mock(Context.class);
    @Mock private final WindowManager mWindowManager = mock(WindowManager.class);
    @Mock private final WindowMetrics mWindowMetrics = mock(WindowMetrics.class);
    @Mock private final DisplayAndroid mDisplay = mock(DisplayAndroid.class);
    private WindowAndroid mWindowAndroid;
    @Spy private WindowAndroid mSpyWindowAndroid;

    @Implements(DisplayAndroid.class)
    static class ShadowDisplayAndroid {
        private static DisplayAndroid sDisplayAndroid;

        public static void setDisplayAndroid(DisplayAndroid displayAndroid) {
            sDisplayAndroid = displayAndroid;
        }

        @Implementation
        public static DisplayAndroid getNonMultiDisplay(Context context) {
            return sDisplayAndroid;
        }
    }

    @Before
    public void setup() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            doReturn(mWindowManager).when(mContext).getSystemService(WindowManager.class);
            doReturn(mWindowMetrics).when(mWindowManager).getCurrentWindowMetrics();
        }

        doReturn(1.0f).when(mDisplay).getDipScale();
        ShadowDisplayAndroid.setDisplayAndroid(mDisplay);

        mWindowAndroid = new WindowAndroid(mContext, false);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testGetBoundsInScreenCoordinates_whenApiLowerThanR_returnsNull() {
        assertNull("Should return null", mWindowAndroid.getBoundsInScreenCoordinates());
    }

    /** Test case for when the context is null. The function should return null. */
    @Test
    public void testGetBoundsInScreenCoordinates_whenContextIsNull_returnsNull() {
        mSpyWindowAndroid = spy(mWindowAndroid);
        doReturn(new WeakReference<Context>(null)).when(mSpyWindowAndroid).getContext();

        assertNull(
                "Should return null if context is null",
                mSpyWindowAndroid.getBoundsInScreenCoordinates());
    }

    /**
     * Test case for the successful path on API level R or higher. The function should return the
     * screen bounds converted to dp.
     */
    @Test
    public void testGetBoundsInScreenCoordinates_returnsBoundsInDp() {
        final Rect boundsPx = new Rect(10, 20, 1090, 1940); // left, top, right, bottom

        doReturn(boundsPx).when(mWindowMetrics).getBounds();
        doReturn(2.0f).when(mDisplay).getDipScale();

        final int[] expectedBoundsDp = new int[] {5, 10, 540, 960}; // x, y, width, height
        final int[] actualBounds = mWindowAndroid.getBoundsInScreenCoordinates();

        assertArrayEquals(
                "Should return correctly converted DP bounds", expectedBoundsDp, actualBounds);
    }
}

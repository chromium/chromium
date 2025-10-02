// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.base.WindowAndroidTest.ShadowDisplayAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetObserver;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = ShadowDisplayAndroid.class, sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
@EnableFeatures({UiAndroidFeatures.ANDROID_USE_CORRECT_WINDOW_BOUNDS})
public class WindowAndroidTest {

    @Mock private final Context mContext = mock(Context.class);
    @Mock private final WindowManager mWindowManager = mock(WindowManager.class);
    @Mock private final WindowMetrics mWindowMetrics = mock(WindowMetrics.class);
    @Mock private final DisplayAndroid mDisplay = mock(DisplayAndroid.class);
    @Mock private final InsetObserver mInsetObserver = mock(InsetObserver.class);

    @Mock
    private final WindowAndroid.Natives mWindowAndroidNativeInterface =
            mock(WindowAndroid.Natives.class);

    private final List<WindowInsetObserver> mWindowInsetObservers = new ArrayList<>();
    private WindowAndroid mWindowAndroid;
    @Spy private WindowAndroid mSpyWindowAndroid;
    private static final Rect INITIAL_WINDOW_BOUNDS = new Rect(10, 20, 1900, 1000);
    private static final long MOCK_NATIVE_POINTER = 1;

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
            doReturn(INITIAL_WINDOW_BOUNDS).when(mWindowMetrics).getBounds();
        }

        doReturn(1.0f).when(mDisplay).getDipScale();
        ShadowDisplayAndroid.setDisplayAndroid(mDisplay);

        doAnswer(
                        invocation -> {
                            final WindowInsetObserver capturedWindowInsetObserver =
                                    invocation.getArgument(0);
                            mWindowInsetObservers.add(capturedWindowInsetObserver);
                            return true;
                        })
                .when(mInsetObserver)
                .addObserver(any(WindowInsetObserver.class));

        WindowAndroidJni.setInstanceForTesting(mWindowAndroidNativeInterface);
        mWindowAndroid = new WindowAndroid(mContext, false, null, mInsetObserver, false);
        mWindowAndroid.setNativePointerForTesting(MOCK_NATIVE_POINTER);
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
        doReturn(new Rect(0, 0, 540, 960)).when(mDisplay).getBounds();

        final int[] expectedBoundsDp = new int[] {5, 10, 540, 960}; // x, y, width, height
        final int[] actualBounds = mWindowAndroid.getBoundsInScreenCoordinates();

        assertArrayEquals(
                "Should return correctly converted DP bounds", expectedBoundsDp, actualBounds);
    }

    /** Test whether the native part is called in the successful path. */
    @Test
    public void testOnWindowPositionChanged_passesToNative() {
        dispatchInsetsChanged();

        verify(mWindowAndroidNativeInterface).onWindowPositionChanged(anyLong());
    }

    /**
     * If the window bounds haven't changed between subsequent inset changes, don't notify the
     * native part for performance reasons.
     */
    @Test
    public void testOnWindowPositionChanged_doesNothingIfBoundsUnchanged() {
        dispatchInsetsChanged();

        verify(mWindowAndroidNativeInterface).onWindowPositionChanged(anyLong());

        dispatchInsetsChanged();

        verify(mWindowAndroidNativeInterface).onWindowPositionChanged(anyLong());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testOnWindowPositionChanged_doesNothingWhenApiLowerThanR() {
        dispatchInsetsChanged();

        verify(mWindowAndroidNativeInterface, never()).onWindowPositionChanged(anyLong());
    }

    private void dispatchInsetsChanged() {
        for (WindowInsetObserver observer : mWindowInsetObservers) {
            observer.onInsetChanged();
        }
    }
}

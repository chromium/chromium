// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class PointerLockTest {

    // Robolectric does not support pointer capture, so record releasePointerCapture() calls.
    private static class TestView extends View {
        int mReleasePointerCaptureCount;

        TestView() {
            super(ContextUtils.getApplicationContext());
            setFocusable(true);
            setFocusableInTouchMode(true);
        }

        @Override
        public void releasePointerCapture() {
            super.releasePointerCapture();
            mReleasePointerCaptureCount++;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid.Natives mWindowAndroidJniMock;
    private final TestView mPointerLockView = new TestView();
    private final View mView = new View(ContextUtils.getApplicationContext());
    private WindowAndroid mWindowAndroid;

    @Before
    public void setup() {
        mWindowAndroid = new WindowAndroid(ContextUtils.getApplicationContext(), false);
        WindowAndroidJni.setInstanceForTesting(mWindowAndroidJniMock);
        mWindowAndroid.setNativePointerForTesting(1L);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    public void testLockPointerViewAndWindowInFocus() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockPointerViewNotInFocus() {
        assertFalse(mPointerLockView.hasFocus());
        assertFalse(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockPointerWindowNotInFocus() {
        assertTrue(mPointerLockView.requestFocus());
        mWindowAndroid.onWindowFocusChanged(false);

        assertFalse(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockAndUnlockPointer() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        mWindowAndroid.releasePointerLock(mPointerLockView);
    }

    @Test
    public void testLockAndUnlockPointerWrongView() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        Assert.assertThrows(AssertionError.class, () -> mWindowAndroid.releasePointerLock(mView));
    }

    @Test
    public void testLockAndUnlockAndRelockPointer() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        mWindowAndroid.releasePointerLock(mPointerLockView);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockPointerTwiceInARow() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));
        Assert.assertThrows(
                AssertionError.class, () -> mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testPointerLockTriggerOnPointerCaptureChangeEvent() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View pointerLockChangeView = mWindowAndroid.getPointerLockChangeViewForTesting();
        assertNotNull(pointerLockChangeView);
        pointerLockChangeView.onPointerCaptureChange(false);

        assertEquals(0, mPointerLockView.mReleasePointerCaptureCount);
        verify(mWindowAndroidJniMock).onWindowPointerLockRelease(anyLong());
    }

    @Test
    public void testPointerLockNotReleasedOnPointerCaptureChangeEvent() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View pointerLockChangeView = mWindowAndroid.getPointerLockChangeViewForTesting();
        assertNotNull(pointerLockChangeView);

        pointerLockChangeView.onPointerCaptureChange(true);

        assertEquals(0, mPointerLockView.mReleasePointerCaptureCount);
        verify(mWindowAndroidJniMock, never()).onWindowPointerLockRelease(anyLong());
    }

    @Test
    public void testPointerLockTriggerOnFocusChange() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View.OnFocusChangeListener focusListener =
                mWindowAndroid.getPointerLockingViewFocusChangeListenerForTesting();
        assertNotNull(focusListener);
        focusListener.onFocusChange(mPointerLockView, false);

        assertEquals(1, mPointerLockView.mReleasePointerCaptureCount);
        verify(mWindowAndroidJniMock).onWindowPointerLockRelease(anyLong());
    }

    @Test
    public void testPointerLockNotReleasedOnFocusChange() {
        assertTrue(mPointerLockView.requestFocus());
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View.OnFocusChangeListener focusListener =
                mWindowAndroid.getPointerLockingViewFocusChangeListenerForTesting();
        assertNotNull(focusListener);
        focusListener.onFocusChange(mPointerLockView, true);

        assertEquals(0, mPointerLockView.mReleasePointerCaptureCount);
        verify(mWindowAndroidJniMock, never()).onWindowPointerLockRelease(anyLong());
    }
}

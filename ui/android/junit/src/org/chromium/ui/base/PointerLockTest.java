// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mPointerLockView;
    @Mock private View mView;
    @Mock private WindowAndroid.Natives mWindowAndroidJniMock;
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
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockPointerViewNotInFocus() {
        when(mPointerLockView.hasFocus()).thenReturn(false);
        assertFalse(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockPointerWindowNotInFocus() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        mWindowAndroid.onWindowFocusChanged(false);

        assertFalse(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockAndUnlockPointer() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        mWindowAndroid.releasePointerLock(mPointerLockView);
    }

    @Test
    public void testLockAndUnlockPointerWrongView() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        Assert.assertThrows(AssertionError.class, () -> mWindowAndroid.releasePointerLock(mView));
    }

    @Test
    public void testLockAndUnlockAndRelockPointer() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        mWindowAndroid.releasePointerLock(mPointerLockView);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testLockPointerTwiceInARow() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));
        Assert.assertThrows(
                AssertionError.class, () -> mWindowAndroid.requestPointerLock(mPointerLockView));
    }

    @Test
    public void testPointerLockTriggerOnPointerCaptureChangeEvent() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View pointerLockChangeView = mWindowAndroid.getPointerLockChangeViewForTesting();
        assertNotNull(pointerLockChangeView);
        pointerLockChangeView.onPointerCaptureChange(false);

        verify(mPointerLockView, never()).releasePointerCapture();
        verify(mWindowAndroidJniMock).onWindowPointerLockRelease(anyLong());
    }

    @Test
    public void testPointerLockNotReleasedOnPointerCaptureChangeEvent() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View pointerLockChangeView = mWindowAndroid.getPointerLockChangeViewForTesting();
        assertNotNull(pointerLockChangeView);

        pointerLockChangeView.onPointerCaptureChange(true);

        verify(mPointerLockView, never()).releasePointerCapture();
        verify(mWindowAndroidJniMock, never()).onWindowPointerLockRelease(anyLong());
    }

    @Test
    public void testPointerLockTriggerOnFocusChange() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View.OnFocusChangeListener focusListener =
                mWindowAndroid.getPointerLockingViewFocusChangeListenerForTesting();
        assertNotNull(focusListener);
        focusListener.onFocusChange(mPointerLockView, false);

        verify(mPointerLockView).releasePointerCapture();
        verify(mWindowAndroidJniMock).onWindowPointerLockRelease(anyLong());
    }

    @Test
    public void testPointerLockNotReleasedOnFocusChange() {
        when(mPointerLockView.hasFocus()).thenReturn(true);
        assertTrue(mWindowAndroid.requestPointerLock(mPointerLockView));

        View.OnFocusChangeListener focusListener =
                mWindowAndroid.getPointerLockingViewFocusChangeListenerForTesting();
        assertNotNull(focusListener);
        focusListener.onFocusChange(mPointerLockView, true);

        verify(mPointerLockView, never()).releasePointerCapture();
        verify(mWindowAndroidJniMock, never()).onWindowPointerLockRelease(anyLong());
    }
}

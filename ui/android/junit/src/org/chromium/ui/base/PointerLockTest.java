// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class PointerLockTest {

    @Mock private View mPointerLockView;
    @Mock private View mView;
    private WindowAndroid mWindowAndroid;

    @Before
    public void setup() {
        mWindowAndroid = new WindowAndroid(ContextUtils.getApplicationContext(), false);
        mPointerLockView = mock(View.class);
        mView = mock(View.class);
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
}

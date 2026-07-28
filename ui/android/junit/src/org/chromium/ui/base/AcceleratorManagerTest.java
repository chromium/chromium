// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;

import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.insets.InsetObserver;

/** Tests for {@link AcceleratorManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
public class AcceleratorManagerTest {
    private final Context mContext = mock(Context.class);
    private final InsetObserver mInsetObserver = mock(InsetObserver.class);

    private WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        mWindowAndroid = new WindowAndroid(mContext, false, null, mInsetObserver, true);
    }

    @After
    public void tearDown() {
        if (!mWindowAndroid.isDestroyed()) {
            mWindowAndroid.destroy();
        }
    }

    @Test
    public void testDestroyDetachesFromWindow() {
        assertNull(AcceleratorManager.fromForTesting(mWindowAndroid));

        AcceleratorManager manager = AcceleratorManager.getOrCreate(mWindowAndroid);
        assertEquals(manager, AcceleratorManager.fromForTesting(mWindowAndroid));

        manager.destroy();
        assertNull(AcceleratorManager.fromForTesting(mWindowAndroid));
    }
}

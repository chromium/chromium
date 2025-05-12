// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class PointerLockEventHelperTest {

    private PointerLockEventHelper mPointerLockEventHelper;

    @Before
    public void setUp() {
        mPointerLockEventHelper = new PointerLockEventHelper();
    }

    @Test
    public void testCapturedPointerTrackpadScrollEvent() {
        MotionEvent event = MotionEventTestUtils.getTrackpadEvent(MotionEvent.ACTION_MOVE, 0, 2);
        MotionEvent updatedEvent =
                PointerLockEventHelper.updateTrackpadCapturedScrollEvent(event, 10, -10);

        assertEquals(MotionEvent.ACTION_SCROLL, updatedEvent.getAction());
        assertTrue(updatedEvent.getAxisValue(MotionEvent.AXIS_HSCROLL) > 0);
        assertTrue(updatedEvent.getAxisValue(MotionEvent.AXIS_VSCROLL) < 0);
    }
}

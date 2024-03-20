// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertEquals;

import android.graphics.Rect;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.List;

/**
 * Test for {@link WindowInsetsUtils#getWidestUnoccludedRect} to fill the gap where Region does not
 * work properly in Robolectric tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class WindowInsetsUtilsJavaUnitTest {

    @Test
    @SmallTest
    public void testGetWidestUnoccludedRect_Horizontal() {
        Rect region = new Rect(0, 0, 600, 800);
        List<Rect> blocks = List.of(new Rect(0, 0, 100, 800), new Rect(400, 0, 600, 800));
        assertEquals(
                new Rect(100, 0, 400, 800),
                WindowInsetsUtils.getWidestUnoccludedRect((region), blocks));
    }

    @Test
    @SmallTest
    public void testGetWidestUnoccludedRect_NoVerticalBlocker() {
        Rect region = new Rect(0, 0, 600, 800);
        List<Rect> blocks = List.of(new Rect(0, 0, 100, 300), new Rect(400, 400, 600, 800));
        assertEquals(
                new Rect(0, 300, 600, 400),
                WindowInsetsUtils.getWidestUnoccludedRect((region), blocks));
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertEquals;

import android.graphics.Rect;
import android.util.Size;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit test for {@link WindowInsetsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WindowInsetsUtilsUnitTest {
    private static final int WINDOW_WIDTH = 600;
    private static final int WINDOW_HEIGHT = 800;
    private static final Rect WINDOW_RECT = new Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    @Test
    public void getRectsFromInsets() {
        Insets topInsets = Insets.of(0, 100, 0, 0);
        assertEquals(
                new Rect(0, 0, 600, 100), WindowInsetsUtils.toRectInWindow(WINDOW_RECT, topInsets));

        Insets leftInsets = Insets.of(100, 0, 0, 0);
        assertEquals(
                new Rect(0, 0, 100, 800),
                WindowInsetsUtils.toRectInWindow(WINDOW_RECT, leftInsets));

        Insets bottomInsets = Insets.of(0, 0, 0, 100);
        assertEquals(
                new Rect(0, 700, 600, 800),
                WindowInsetsUtils.toRectInWindow(WINDOW_RECT, bottomInsets));

        Insets rightInsets = Insets.of(0, 0, 100, 0);
        assertEquals(
                new Rect(500, 0, 600, 800),
                WindowInsetsUtils.toRectInWindow(WINDOW_RECT, rightInsets));
    }

    @Test
    public void getRectsFromInsets_InvalidInputs() {
        Insets insets = Insets.of(100, 100, 0, 0);
        assertEquals(new Rect(), WindowInsetsUtils.toRectInWindow(WINDOW_RECT, insets));

        insets = Insets.of(100, 100, 100, 0);
        assertEquals(new Rect(), WindowInsetsUtils.toRectInWindow(WINDOW_RECT, insets));

        insets = Insets.of(100, 100, 100, 100);
        assertEquals(new Rect(), WindowInsetsUtils.toRectInWindow(WINDOW_RECT, insets));

        insets = Insets.of(0, 0, 0, 0);
        assertEquals(new Rect(), WindowInsetsUtils.toRectInWindow(WINDOW_RECT, insets));
    }

    @Test
    public void getFrame_NullInsets() {
        assertEquals(
                "Frame size is incorrect.",
                new Size(0, 0),
                WindowInsetsUtils.getFrameFromInsets(null));
    }

    @Test
    public void getBoundingRects_NullInsets() {
        assertEquals(
                "Bounding rects list is incorrect.",
                List.of(),
                WindowInsetsUtils.getBoundingRectsFromInsets(
                        null, WindowInsetsCompat.Type.captionBar()));
    }
}

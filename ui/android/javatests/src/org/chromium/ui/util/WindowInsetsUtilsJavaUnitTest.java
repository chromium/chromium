// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertEquals;

import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.util.Size;
import android.view.WindowInsets;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.MinAndroidSdkLevel;

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
                WindowInsetsUtils.getWidestUnoccludedRect(region, blocks));
    }

    @Test
    @SmallTest
    public void testGetWidestUnoccludedRect_NoVerticalBlocker() {
        Rect region = new Rect(0, 0, 600, 800);
        List<Rect> blocks = List.of(new Rect(0, 0, 100, 300), new Rect(400, 400, 600, 800));
        assertEquals(
                new Rect(0, 300, 600, 400),
                WindowInsetsUtils.getWidestUnoccludedRect(region, blocks));
    }

    @Test
    @SmallTest
    @MaxAndroidSdkLevel(VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testGetFrame_PreV() {
        WindowInsets insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.captionBar(), Insets.of(0, 100, 0, 0))
                        .build()
                        .toWindowInsets();
        assertEquals(
                "Frame size is incorrect.",
                new Size(0, 0),
                WindowInsetsUtils.getFrameFromInsets(insets));
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(VERSION_CODES.VANILLA_ICE_CREAM)
    public void testGetFrame_PostV() {
        int frameWidth = 1600;
        int frameHeight = 1200;
        var insets = new WindowInsets.Builder().setFrame(frameWidth, frameHeight).build();
        assertEquals(
                "Frame size is incorrect.",
                new Size(frameWidth, frameHeight),
                WindowInsetsUtils.getFrameFromInsets(insets));
    }

    @Test
    @SmallTest
    @MaxAndroidSdkLevel(VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testGetBoundingRects_PreV() {
        WindowInsets insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.captionBar(), Insets.of(0, 100, 0, 0))
                        .build()
                        .toWindowInsets();
        assertEquals(
                "Bounding rects list is incorrect.",
                List.of(),
                WindowInsetsUtils.getBoundingRectsFromInsets(
                        insets, WindowInsetsCompat.Type.captionBar()));
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(VERSION_CODES.VANILLA_ICE_CREAM)
    public void testGetBoundingRects_PostV() {
        var boundingRects = List.of(new Rect(0, 0, 100, 100), new Rect(800, 0, 1600, 100));
        var insets =
                new WindowInsets.Builder()
                        .setBoundingRects(WindowInsetsCompat.Type.captionBar(), boundingRects)
                        .build();
        assertEquals(
                "Bounding rects list is incorrect.",
                boundingRects,
                WindowInsetsUtils.getBoundingRectsFromInsets(
                        insets, WindowInsetsCompat.Type.captionBar()));
    }
}

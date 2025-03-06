// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.ui.util.ColorBlendAnimationFactory.createColorBlendAnimation;
import static org.chromium.ui.util.ColorBlendAnimationFactory.createMultiColorBlendAnimation;

import android.graphics.Color;
import android.os.Looper;

import androidx.annotation.ColorInt;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link ColorBlendAnimationFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class ColorBlendAnimationFactoryTest {
    private static final int[] COLORS_ALPHA_OVER_ZERO = {
        Color.parseColor("#FF000000"),
        Color.parseColor("#FF123456"),
        Color.parseColor("#FFFFFFFF"),
        Color.parseColor("#22446688"),
        Color.parseColor("#88888888"),
        Color.parseColor("#12345678"),
    };

    private static final int[] COLORS_ALPHA_ZERO = {
        Color.TRANSPARENT, Color.parseColor("#00123456"), Color.parseColor("#00FFFFFF"),
    };

    @Test
    public void testColorBlendAnimation_AlphaOverZero() {
        for (@ColorInt int start : COLORS_ALPHA_OVER_ZERO) {
            for (@ColorInt int end : COLORS_ALPHA_OVER_ZERO) {
                AtomicInteger latestColor = new AtomicInteger();
                createColorBlendAnimation(10, start, end, latestColor::set).start();

                Shadows.shadowOf(Looper.getMainLooper()).idle();
                assertEquals(end, latestColor.get());
            }
        }
    }

    @Test
    public void testMultiColorBlendAnimation_AlphaOverZero() {
        for (@ColorInt int start : COLORS_ALPHA_OVER_ZERO) {
            for (@ColorInt int end : COLORS_ALPHA_OVER_ZERO) {
                AtomicReference<int[]> latestColors = new AtomicReference<>();
                createMultiColorBlendAnimation(
                                10,
                                new int[] {start, end},
                                new int[] {end, start},
                                latestColors::set)
                        .start();

                Shadows.shadowOf(Looper.getMainLooper()).idle();

                int[] expectedEndColors = new int[] {end, start};
                int[] actualEndColors = latestColors.get();

                assertNotNull(actualEndColors);
                assertArrayEquals(expectedEndColors, actualEndColors);
            }
        }
    }

    @Test
    public void testColorBlendAnimation_AlphaZero() {
        for (@ColorInt int start : COLORS_ALPHA_ZERO) {
            for (@ColorInt int end : COLORS_ALPHA_ZERO) {
                AtomicInteger latestColor = new AtomicInteger();
                createColorBlendAnimation(10, start, end, latestColor::set).start();

                Shadows.shadowOf(Looper.getMainLooper()).idle();
                assertEquals(Color.TRANSPARENT, latestColor.get());
            }
        }
    }

    @Test
    public void testMultiColorBlendAnimation_AlphaZero() {
        for (@ColorInt int start : COLORS_ALPHA_ZERO) {
            for (@ColorInt int end : COLORS_ALPHA_ZERO) {
                AtomicReference<int[]> latestColors = new AtomicReference<>();
                createMultiColorBlendAnimation(
                                10,
                                new int[] {start, end},
                                new int[] {end, start},
                                latestColors::set)
                        .start();

                Shadows.shadowOf(Looper.getMainLooper()).idle();

                int[] expectedEndColors = new int[] {Color.TRANSPARENT, Color.TRANSPARENT};
                int[] actualEndColors = latestColors.get();

                assertNotNull(actualEndColors);
                assertArrayEquals(expectedEndColors, actualEndColors);
            }
        }
    }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertTrue;

import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.FloatRange;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ColorUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ColorUtilsTest {
    // Use fixed fractions to avoid any float addition that might not exactly hit 0f and 1f.
    private static final float[] FRACTIONS = {0f, .1f, .2f, .3f, .4f, .5f, .6f, .7f, .8f, .9f, 1f};

    // These cannot have any alpha.
    private static final int[] BACKGROUNDS = {
        Color.parseColor("#FFFFFF"),
        Color.parseColor("#000000"),
        Color.parseColor("#115599"),
        Color.parseColor("#888888")
    };

    private static final int[] COLORS = {
        Color.parseColor("#00000000"),
        Color.parseColor("#00123456"),
        Color.parseColor("#00FFFFFF"),
        Color.parseColor("#FF000000"),
        Color.parseColor("#FF123456"),
        Color.parseColor("#FFFFFFFF"),
        Color.parseColor("#22446688"),
        Color.parseColor("#88888888"),
        Color.parseColor("#12345678"),
    };

    @Test
    public void testBlendColorsMultiply() {
        for (@ColorInt int background : BACKGROUNDS) {
            for (@ColorInt int from : COLORS) {
                for (@ColorInt int to : COLORS) {
                    testBlendColorsMultiplyHelper(background, from, to);
                }
            }
        }
    }

    private void testBlendColorsMultiplyHelper(
            @ColorInt int background, @ColorInt int from, @ColorInt int to) {
        String sharedMessage = formatColors("background:%s from:%s to:%s", background, from, to);
        // Calculates an expected color by pre-flattening everything onto the background and an
        // actual value by using the pre multiply blend mechanism that combines two colors with
        // alpha values. These two approaches should return the same color.
        @ColorInt int fromFlat = flatten(background, from);
        @ColorInt int toFlat = flatten(background, to);
        for (@FloatRange(from = 0f, to = 1f) float fraction : FRACTIONS) {
            @ColorInt int flatBlend = ColorUtils.getColorWithOverlay(fromFlat, toFlat, fraction);
            @ColorInt int blend = ColorUtils.blendColorsMultiply(from, to, fraction);
            @ColorInt int blendOnBackground = flatten(background, blend);
            String fractionMessage = String.format("%s fraction:%s", sharedMessage, fraction);
            assertColorsEqual(fractionMessage, flatBlend, blendOnBackground);
        }
    }

    private @ColorInt int flatten(@ColorInt int background, @ColorInt int overlay) {
        assert Color.alpha(background) == 255;
        return ColorUtils.getColorWithOverlay(background, overlay, Color.alpha(overlay) / 255f);
    }

    private void assertColorsEqual(
            String testMessage, @ColorInt int expected, @ColorInt int actual) {
        String compareMessage =
                String.format(
                        "%s expected:%s actual:%s",
                        testMessage, printColor(expected), printColor(actual));
        // Allow for rounding errors where things are off by 1.
        assertTrue(compareMessage, Math.abs(Color.red(expected) - Color.red(actual)) <= 1);
        assertTrue(compareMessage, Math.abs(Color.green(expected) - Color.green(actual)) <= 1);
        assertTrue(compareMessage, Math.abs(Color.blue(expected) - Color.blue(actual)) <= 1);
        assertTrue(compareMessage, Math.abs(Color.alpha(expected) - Color.alpha(actual)) <= 1);
    }

    private String printColor(@ColorInt int color) {
        return String.format(
                "{r:%s,g:%s,b:%s,a:%s",
                Color.red(color), Color.green(color), Color.blue(color), Color.alpha(color));
    }

    private String formatColors(String formatString, int... colors) {
        int length = colors.length;
        Object[] colorStrings = new String[length];
        for (int i = 0; i < length; ++i) {
            colorStrings[i] = printColor(colors[i]);
        }
        return String.format(formatString, colorStrings);
    }
}

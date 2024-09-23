// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertEquals;
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

    @Test
    public void testGetOpaqueColor() {
        for (@ColorInt int input : COLORS) {
            @ColorInt int opaqueColor = ColorUtils.getOpaqueColor(input);
            assertRbgExactlyEqual("", opaqueColor, input);
            assertEquals(255, Color.alpha(opaqueColor));
        }
    }

    @Test
    public void testOverlayTransparentColor() {
        // Hard coded solved expected color to avoid just duplicating impl in the test.
        @ColorInt int expected = Color.parseColor("#FF143658");
        @ColorInt int base = Color.parseColor("#FF123456");
        @ColorInt int overlay = Color.parseColor("#12345678");
        @ColorInt int actual = ColorUtils.overlayColor(base, overlay);
        assertColorsExactlyEqual("", expected, actual);
    }

    @Test
    public void testOverlayTransparentColorWithFraction() {
        // Hard coded solved expected color to avoid just duplicating impl in the test.
        @ColorInt int expected = Color.parseColor("#FF143658");
        @ColorInt int base = Color.parseColor("#FF123456");
        @ColorInt int overlay = Color.parseColor("#12345678");
        @ColorInt int actual = ColorUtils.overlayColor(base, overlay, .63f);
        assertColorsExactlyEqual("", expected, actual);
    }

    @Test
    public void testCalculateLuminance() {
        assertEquals(
                "Color.BLACK should have a luminance of 0.",
                0.0f,
                ColorUtils.calculateLuminance(Color.BLACK),
                0.01f);
        assertEquals(
                "Color.WHITE should have a luminance of 1.",
                1.0f,
                ColorUtils.calculateLuminance(Color.WHITE),
                0.01f);
        assertEquals(
                "Color.GRAY should have a luminance of 0.53125.",
                0.53125f,
                ColorUtils.calculateLuminance(Color.GRAY),
                0.01f);
        assertEquals(
                "Color.GREEN should have a luminance of about 0.71875.",
                0.71875f,
                ColorUtils.calculateLuminance(Color.GREEN),
                0.01f);
        assertEquals(
                "Color.CYAN should have a luminance of 0.78125.",
                0.78125f,
                ColorUtils.calculateLuminance(Color.CYAN),
                0.01f);
        assertEquals(
                "Color.YELLOW should have a luminance of 0.9375.",
                0.9375f,
                ColorUtils.calculateLuminance(Color.YELLOW),
                0.01f);
    }

    private void testBlendColorsMultiplyHelper(
            @ColorInt int background, @ColorInt int from, @ColorInt int to) {
        String sharedMessage = formatColors("background:%s from:%s to:%s", background, from, to);
        // Calculates an expected color by pre-flattening everything onto the background and an
        // actual value by using the pre multiply blend mechanism that combines two colors with
        // alpha values. These two approaches should return the same color.
        @ColorInt int fromFlat = ColorUtils.overlayColor(background, from);
        @ColorInt int toFlat = ColorUtils.overlayColor(background, to);
        for (@FloatRange(from = 0f, to = 1f) float fraction : FRACTIONS) {
            @ColorInt int flatBlend = ColorUtils.getColorWithOverlay(fromFlat, toFlat, fraction);
            @ColorInt int blend = ColorUtils.blendColorsMultiply(from, to, fraction);
            @ColorInt int blendOnBackground = ColorUtils.overlayColor(background, blend);
            String fractionMessage = String.format("%s fraction:%s", sharedMessage, fraction);
            // Use a delta to allow rounding errors where things are off by 1.
            assertColorsNearlyEqual(fractionMessage, flatBlend, blendOnBackground, /* delta= */ 1);
        }
    }

    private void assertRbgExactlyEqual(
            String testMessage, @ColorInt int expected, @ColorInt int actual) {
        String compareMessage =
                String.format(
                        "%s expected:%s actual:%s",
                        testMessage, printColor(expected), printColor(actual));

        assertEquals(compareMessage, Color.red(expected), Color.red(actual));
        assertEquals(compareMessage, Color.green(expected), Color.green(actual));
        assertEquals(compareMessage, Color.blue(expected), Color.blue(actual));
    }

    private void assertColorsExactlyEqual(
            String testMessage, @ColorInt int expected, @ColorInt int actual) {
        assertColorsNearlyEqual(testMessage, expected, actual, /* delta= */ 0);
    }

    private void assertColorsNearlyEqual(
            String testMessage, @ColorInt int expected, @ColorInt int actual, int delta) {
        String compareMessage =
                String.format(
                        "%s expected:%s actual:%s",
                        testMessage, printColor(expected), printColor(actual));

        assertTrue(compareMessage, Math.abs(Color.red(expected) - Color.red(actual)) <= delta);
        assertTrue(compareMessage, Math.abs(Color.green(expected) - Color.green(actual)) <= delta);
        assertTrue(compareMessage, Math.abs(Color.blue(expected) - Color.blue(actual)) <= delta);
        assertTrue(compareMessage, Math.abs(Color.alpha(expected) - Color.alpha(actual)) <= delta);
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

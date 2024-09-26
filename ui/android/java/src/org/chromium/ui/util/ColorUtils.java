// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.FloatRange;
import androidx.annotation.IntRange;

import org.chromium.base.MathUtils;

/** Helper functions for working with colors. */
public class ColorUtils {
    // Value used by ui::OptionalSkColorToJavaColor() to represent invalid color.
    public static final long INVALID_COLOR = ((long) Integer.MAX_VALUE) + 1;

    private static final float CONTRAST_LIGHT_ITEM_THRESHOLD = 3f;
    private static final float LIGHTNESS_OPAQUE_BOX_THRESHOLD = 0.82f;
    private static final float MAX_LUMINANCE_FOR_VALID_THEME_COLOR = 0.94f;
    private static final float THEMED_FOREGROUND_BLACK_FRACTION = 0.64f;
    private static final float LIGHT_DARK_LUMINANCE_THRESHOLD = 0.5f;

    /** Percentage to darken a color by when setting the status bar color. */
    private static final float DARKEN_COLOR_FRACTION = 0.6f;

    /**
     * @param context <b>Activity</b> context.
     * @return Whether the activity is currently in night mode.
     */
    public static boolean inNightMode(Context context) {
        int uiMode = context.getResources().getConfiguration().uiMode;
        return (uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    }

    /** Computes the lightness value in HSL standard for the given color. */
    public static float getLightnessForColor(@ColorInt int color) {
        int red = Color.red(color);
        int green = Color.green(color);
        int blue = Color.blue(color);
        int largest = Math.max(red, Math.max(green, blue));
        int smallest = Math.min(red, Math.min(green, blue));
        int average = (largest + smallest) / 2;
        return average / 255.0f;
    }

    /**
     * Calculates the contrast between the given color and white, using the algorithm provided by
     * the WCAG v2 in http://www.w3.org/TR/WCAG20/#contrast-ratiodef.
     */
    private static float getContrastForColor(@ColorInt int color) {
        float bgR = Color.red(color) / 255f;
        float bgG = Color.green(color) / 255f;
        float bgB = Color.blue(color) / 255f;
        bgR = (bgR < 0.03928f) ? bgR / 12.92f : (float) Math.pow((bgR + 0.055f) / 1.055f, 2.4f);
        bgG = (bgG < 0.03928f) ? bgG / 12.92f : (float) Math.pow((bgG + 0.055f) / 1.055f, 2.4f);
        bgB = (bgB < 0.03928f) ? bgB / 12.92f : (float) Math.pow((bgB + 0.055f) / 1.055f, 2.4f);
        float bgL = 0.2126f * bgR + 0.7152f * bgG + 0.0722f * bgB;
        return Math.abs(1.05f / (bgL + 0.05f));
    }

    /**
     * @see ColorUtils#overlayColor(int, int, float). Use this when not in an animation.
     */
    public static @ColorInt int overlayColor(@ColorInt int baseColor, @ColorInt int overlayColor) {
        return overlayColor(baseColor, overlayColor, /* fraction= */ 1f);
    }

    /**
     * Overlays a likely transparent color with the amount that it is transparent. This effectively
     * flattens the two colors together into a new opaque color.
     *
     * @param baseColor The base, opaque, color that is beneath the overlay.
     * @param overlayColor The partially transparent color, whose alpha will be used to decide how
     *     much of an effect it will have when blending.
     * @param fraction Extra 0 to 1 multiplier for alpha overlay, useful for animations that want to
     *     animate into a full overlay.
     * @return A fully opaque color that's the result of blending the two.
     */
    public static @ColorInt int overlayColor(
            @ColorInt int baseColor,
            @ColorInt int overlayColor,
            @FloatRange(from = 0f, to = 1f) float fraction) {
        // Similar to #getColorWithOverlay, this math isn't great for transparent base colors.
        // Consider using #blendColorsMultiply instead.
        // TODO(crbug.com/40282487): Enable asserts once status bar stops passing a base
        // color that's partially transparent.
        // assert Color.alpha(baseColor) == 255;

        @FloatRange(from = 0f, to = 1f)
        float alphaAdjustedFraction = Color.alpha(overlayColor) / 255f * fraction;
        @ColorInt int opaqueOverlayColor = getOpaqueColor(overlayColor);
        return getColorWithOverlay(baseColor, opaqueOverlayColor, alphaAdjustedFraction);
    }

    /**
     * Get a color when overlaid with a different color. Input and output colors should be fully
     * opaque, as this approach does not work well with transparency.
     *
     * @param baseColor The base Android color.
     * @param overlayColor The overlay Android color.
     * @param overlayAlpha The alpha |overlayColor| should have on the base color.
     */
    public static @ColorInt int getColorWithOverlay(
            @ColorInt int baseColor,
            @ColorInt int overlayColor,
            @FloatRange(from = 0f, to = 1f) float overlayAlpha) {
        // Transparency is ignored in the logic below, so assert if anyone is passing a color that's
        // not fully opaque. This does incur a minor burden on clients that knowingly want to call
        // this on a partially transparent color, as they have to change the alpha value first.
        // TODO(crbug.com/40282487): Enable asserts once status bar stops passing a base
        // color that's partially transparent.
        // assert Color.alpha(baseColor) == 255;
        assert Color.alpha(overlayColor) == 255;

        int fromRed = Color.red(baseColor);
        int toRed = Color.red(overlayColor);
        int resultRed = Math.round(MathUtils.interpolate(fromRed, toRed, overlayAlpha));

        int fromGreen = Color.green(baseColor);
        int toGreen = Color.green(overlayColor);
        int resultGreen = Math.round(MathUtils.interpolate(fromGreen, toGreen, overlayAlpha));

        int fromBlue = Color.blue(baseColor);
        int toBlue = Color.blue(overlayColor);
        int resultBlue = Math.round(MathUtils.interpolate(fromBlue, toBlue, overlayAlpha));

        return Color.rgb(resultRed, resultGreen, resultBlue);
    }

    /**
     * Darkens the given color to use on the status bar.
     *
     * @param color Color which should be darkened.
     * @return Color that should be used for Android status bar.
     */
    public static @ColorInt int getDarkenedColorForStatusBar(@ColorInt int color) {
        return getDarkenedColor(color, DARKEN_COLOR_FRACTION);
    }

    /**
     * Darken a color to a fraction of its current brightness.
     *
     * @param color The input color.
     * @param darkenFraction The fraction of the current brightness the color should be.
     * @return The new darkened color.
     */
    public static @ColorInt int getDarkenedColor(@ColorInt int color, float darkenFraction) {
        float[] hsv = new float[3];
        Color.colorToHSV(color, hsv);
        hsv[2] *= darkenFraction;
        return Color.HSVToColor(hsv);
    }

    /**
     * Check whether lighter or darker foreground elements (i.e. text, drawables etc.) should be
     * used depending on the given background color.
     *
     * @param backgroundColor The background color value which is being queried.
     * @return Whether light colored elements should be used.
     */
    public static boolean shouldUseLightForegroundOnBackground(@ColorInt int backgroundColor) {
        return getContrastForColor(backgroundColor) >= CONTRAST_LIGHT_ITEM_THRESHOLD;
    }

    /**
     * Check which version of the textbox background should be used depending on the given color.
     *
     * @param color The color value we are querying for.
     * @return Whether the transparent version of the background should be used.
     */
    public static boolean shouldUseOpaqueTextboxBackground(@ColorInt int color) {
        return getLightnessForColor(color) > LIGHTNESS_OPAQUE_BOX_THRESHOLD;
    }

    /**
     * Returns an opaque version of the given color.
     *
     * @param color Color for which an opaque version should be returned.
     * @return Opaque version of the given color.
     */
    public static @ColorInt int getOpaqueColor(@ColorInt int color) {
        return color | 0xFF000000;
    }

    /**
     * Determine if a theme color is too bright. A theme color is too bright if its luminance is >
     * 0.94.
     *
     * @param color The color to test.
     * @return True if the theme color is too bright.
     */
    public static boolean isThemeColorTooBright(@ColorInt int color) {
        return ColorUtils.getLightnessForColor(color) > MAX_LUMINANCE_FOR_VALID_THEME_COLOR;
    }

    /**
     * Compute a color to use for assets that sit on top of a themed background.
     *
     * @param themeColor The base theme color.
     * @return A color to use for elements in the foreground (on top of the base theme color).
     */
    public static @ColorInt int getThemedAssetColor(@ColorInt int themeColor, boolean isIncognito) {
        if (ColorUtils.shouldUseLightForegroundOnBackground(themeColor) || isIncognito) {
            // Dark theme.
            return Color.WHITE;
        } else {
            // Light theme.
            return ColorUtils.getColorWithOverlay(
                    themeColor, Color.BLACK, THEMED_FOREGROUND_BLACK_FRACTION);
        }
    }

    /**
     * Interpolates between two colors, using pre-multiplied alpha values. Tries to not allocate any
     * new objects or lose any precision unnecessarily.
     *
     * @param from The color to start at, when fraction is at zero.
     * @param to The color to end at, when the fraction is at one.
     * @param fraction The percent through interpolation that's currently being calculated.
     * @return The interpolated color value.
     */
    public static @ColorInt int blendColorsMultiply(
            @ColorInt int from, @ColorInt int to, @FloatRange(from = 0f, to = 1f) float fraction) {
        int fromAlpha = Color.alpha(from);
        int toAlpha = Color.alpha(to);
        // Alpha can be linearly interpolated. Keep the result as float to increase precision of
        // the intermediate math. Lastly, this alpha value can be zero, and we're going to divide by
        // it below. Surprisingly, no special casing is needed. If resultAlpha is zero, this is
        // because one or both of the from/to alphas are also zero, and the color channel
        // interpolation is always going to get zero back. Turns out in Java, 0.0f / 0.0f is NaN,
        // and Math.round special cases this to return 0, which is what we'd want to do anyway.
        float resultAlpha = MathUtils.interpolate(fromAlpha, toAlpha, fraction);

        // Each rgb channel value is multiplied by source alpha before interpolation. Then it'll be
        // divided by the result alpha at the end. This is the pre-multiplied alpha approach as
        // detailed in https://en.wikipedia.org/wiki/Alpha_compositing#Alpha_blending.
        int fromRed = Color.red(from) * fromAlpha;
        int toRed = Color.red(to) * toAlpha;
        int resultRed = Math.round(MathUtils.interpolate(fromRed, toRed, fraction) / resultAlpha);

        int fromGreen = Color.green(from) * fromAlpha;
        int toGreen = Color.green(to) * toAlpha;
        int resultGreen =
                Math.round(MathUtils.interpolate(fromGreen, toGreen, fraction) / resultAlpha);

        int fromBlue = Color.blue(from) * fromAlpha;
        int toBlue = Color.blue(to) * toAlpha;
        int resultBlue =
                Math.round(MathUtils.interpolate(fromBlue, toBlue, fraction) / resultAlpha);

        return Color.argb(Math.round(resultAlpha), resultRed, resultGreen, resultBlue);
    }

    /**
     * Pass through to {@link androidx.core.graphics.ColorUtils#setAlphaComponent(int, int)} so
     * callers that need methods out of this class don't need to bother importing two versions of
     * classes named "ColorUtils".
     */
    public static @ColorInt int setAlphaComponent(
            @ColorInt int color, @IntRange(from = 0L, to = 255L) int alpha) {
        return androidx.core.graphics.ColorUtils.setAlphaComponent(color, alpha);
    }

    /**
     * Convert the float alpha value into an integer ranging from 0 to 255 to be used in {@link
     * #setAlphaComponent(int, int)}
     */
    public static @ColorInt int setAlphaComponentWithFloat(
            @ColorInt int color, @FloatRange(from = 0f, to = 1f) float alpha) {
        return setAlphaComponent(color, (int) (alpha * 255));
    }

    /**
     * Calculates the luminosity for the given color. This should match Android's region sampling
     * calculation (go/android-luma-calculation).
     *
     * @return The luminance percentage for the color, with {@link Color.BLACK} having a luminance
     *     of 0 and {@link Color.WHITE} have a luminance of 1.
     */
    public static float calculateLuminance(@ColorInt int color) {
        int b = color & 0xFF;
        int g = (color >> 8) & 0xFF;
        int r = (color >> 16) & 0xFF;
        int luminance = (r * 7 + b * 2 + g * 23) / 32;
        return luminance / 255.0f;
    }

    /**
     * Determines whether the luminance is overall high (light) or low (dark). This follows
     * Android's NAVIGATION_LUMINANCE_THRESHOLD.
     */
    public static boolean isHighLuminance(float luminance) {
        return luminance >= LIGHT_DARK_LUMINANCE_THRESHOLD;
    }
}

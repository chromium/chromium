// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.chromium.ui.util.ColorUtils.blendColorsMultiply;

import android.animation.Animator;
import android.animation.ValueAnimator;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;

import java.util.function.Consumer;

/** Factory class for creating animations for blending two colors together. */
@NullMarked
public class ColorBlendAnimationFactory {

    /**
     * Creates an animation for blending two colors together.
     *
     * @param duration The duration of the animation in milliseconds.
     * @param startColor The starting color in the animation.
     * @param endColor The color to transition towards in the animation.
     * @param onUpdate A consumer which uses an interpolated color on each animation update.
     */
    public static Animator createColorBlendAnimation(
            long duration,
            @ColorInt int startColor,
            @ColorInt int endColor,
            Consumer<Integer> onUpdate) {
        ValueAnimator animator = ValueAnimator.ofFloat(0, 1);
        animator.setDuration(duration);
        animator.addUpdateListener(
                animation -> {
                    float fraction = animation.getAnimatedFraction();
                    @ColorInt int color = blendColorsMultiply(startColor, endColor, fraction);
                    onUpdate.accept(color);
                });
        return animator;
    }

    /**
     * Creates an animation for blending multiple pairs of colors concurrently. On each animator
     * update, each element at a given index in startColors will be transitioned towards the element
     * at the same index in endColors. The array containing these interpolated colors will be
     * accessible at each animator update via a consumer.
     *
     * @param duration The duration of the animation in milliseconds.
     * @param startColors The starting colors in the animation.
     * @param endColors The colors to transition towards in the animation.
     * @param onUpdate A consumer which uses an array of interpolated colors on each animation
     *     update.
     */
    public static Animator createMultiColorBlendAnimation(
            long duration,
            @ColorInt int[] startColors,
            @ColorInt int[] endColors,
            Consumer<int[]> onUpdate) {
        assert startColors.length == endColors.length;
        @ColorInt int[] buffer = new int[startColors.length];

        ValueAnimator animator = ValueAnimator.ofFloat(0, 1);
        animator.setDuration(duration);
        animator.addUpdateListener(
                animation -> {
                    float fraction = animation.getAnimatedFraction();
                    for (int i = 0; i < buffer.length; i++) {
                        buffer[i] = blendColorsMultiply(startColors[i], endColors[i], fraction);
                    }
                    onUpdate.accept(buffer);
                });
        return animator;
    }
}

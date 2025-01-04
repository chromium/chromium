// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.animation.Animator;
import android.animation.ValueAnimator;

import org.chromium.build.annotations.NullMarked;

/** Factory class for creating animations for translations. */
@NullMarked
public class TranslationAnimatorFactory {

    @FunctionalInterface
    public interface TranslationAnimationOnUpdate {
        void onUpdate(float x, float y);
    }

    /**
     * Creates an animation for translations. The animator will begin at the maximum displacement
     * specified and will return to a the non-translated position.
     *
     * <p>It is preferred that {@link android.animation.ObjectAnimator} or {@link
     * android.animation.ValueAnimator} are used for {@link android.view.View} elements as well as
     * those with existing animation properties. This is more well-suited for {@link
     * android.graphics.drawable.Drawable} elements or other data types without animation
     * properties.
     *
     * @param dX The amount the drawable is translated horizontally (unit agnostic).
     * @param dY The amount the drawable is translated vertically (unit agnostic).
     * @param consumer a consumer which can accept the current total 2-D displacement on each
     *     animator update.
     */
    public static Animator buildTranslationAnimation(
            int dX, int dY, TranslationAnimationOnUpdate consumer) {
        ValueAnimator animator = ValueAnimator.ofFloat(0, 1);
        animator.addUpdateListener(
                animation -> {
                    float fraction = animation.getAnimatedFraction();
                    float x = (1 - fraction) * dX;
                    float y = (1 - fraction) * dY;
                    consumer.onUpdate(x, y);
                });
        return animator;
    }
}

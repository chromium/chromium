// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.chromium.ui.animation.AnimationListeners.onAnimationEnd;
import static org.chromium.ui.animation.AnimationListeners.onAnimationStart;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.graphics.Path;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.animation.PathAnimationUtils.ArcDirection;

/** A factory for creating commonly used animations. */
@NullMarked
public class CommonAnimationsFactory {

    /**
     * Creates an animator that fades a view in by animating its alpha from 0 to 1. Sets the view's
     * visibility to {@link View#VISIBLE} at the start of the animation.
     *
     * @param view The view to fade in.
     */
    public static Animator createFadeInAnimation(View view) {
        ObjectAnimator fadeIn = ObjectAnimator.ofFloat(view, View.ALPHA, 0f, 1f);
        fadeIn.addListener(onAnimationStart(ignored -> view.setVisibility(VISIBLE)));
        return fadeIn;
    }

    /**
     * Creates an animator that fades a view out by animating its alpha from 1 to 0. Sets the view's
     * visibility to {@link View#GONE} at the end of the animation.
     *
     * @param view The view to fade out.
     */
    public static Animator createFadeOutAnimation(View view) {
        ObjectAnimator fadeOut = ObjectAnimator.ofFloat(view, View.ALPHA, 1f, 0f);
        fadeOut.addListener(
                onAnimationEnd(
                        ignored -> {
                            view.setVisibility(GONE);
                            view.setAlpha(1f);
                        }));
        return fadeOut;
    }

    /**
     * Creates an animator that translates a view along an arc path.
     *
     * @param view The view to animate.
     * @param startX Starting X coordinate.
     * @param startY Starting Y coordinate.
     * @param endX Ending X coordinate.
     * @param endY Ending Y coordinate.
     * @param direction The {@link ArcDirection} (clockwise or counter-clockwise).
     * @return An {@link Animator} animating {@link View#X} and {@link View#Y}.
     */
    public static Animator createViewArcAnimation(
            View view,
            float startX,
            float startY,
            float endX,
            float endY,
            @ArcDirection int direction) {
        Path path = PathAnimationUtils.createArcPath(startX, startY, endX, endY, direction);
        return ObjectAnimator.ofFloat(view, View.X, View.Y, path);
    }
}

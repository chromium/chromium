// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation.transition;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.graphics.Path;
import android.transition.TransitionValues;
import android.transition.Visibility;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;

/**
 * A Transition that changes a view's scale when it's appearing or disappearing. This Transition
 * does not hold a strong reference to the View it's animating, so it is the responsibility of the
 * caller to hold a strong reference throughout the duration of the Transition animation to ensure
 * nothing breaks.
 */
@NullMarked
public class ShrinkTransition extends Visibility {
    private final float mGoneScale;
    private final float mVisibleScale;

    /**
     * Creates a ShrinkTransition that animates between the given scales.
     *
     * @param goneScale The scale of the view when it is not visible.
     * @param visibleScale The scale of the view when it is visible.
     */
    public ShrinkTransition(float goneScale, float visibleScale) {
        mGoneScale = goneScale;
        mVisibleScale = visibleScale;
    }

    /** Creates a ShrinkTransition that animates between 0 and 1. */
    public ShrinkTransition() {
        this(0f, 1f);
    }

    @Override
    public Animator onAppear(
            ViewGroup sceneRoot,
            View view,
            TransitionValues startValues,
            TransitionValues endValues) {
        return createAnimation(view, mGoneScale, mVisibleScale);
    }

    @Override
    public Animator onDisappear(
            ViewGroup sceneRoot,
            View view,
            TransitionValues startValues,
            TransitionValues endValues) {
        return createAnimation(view, mVisibleScale, mGoneScale);
    }

    private Animator createAnimation(final View view, float startScale, float endScale) {
        Path animationPath = new Path();
        animationPath.moveTo(startScale, startScale);
        animationPath.lineTo(endScale, endScale);

        final ObjectAnimator animator =
                ObjectAnimator.ofFloat(view, "ScaleX", "ScaleY", animationPath);

        if (endScale == mGoneScale) {
            animator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            // We reset the scale to the visible scale at the end of a disappear
                            // animation. This ensures that if the view is shown again later without
                            // this transition, it will correctly start at its full visible scale
                            // rather than the shrunken scale it finished at.
                            view.setScaleX(mVisibleScale);
                            view.setScaleY(mVisibleScale);

                            super.onAnimationEnd(animation);
                        }
                    });
        }

        return animator;
    }
}

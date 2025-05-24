// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * Keeps track of animations. Helps to ensure only one instance of this Animation is running at a
 * time.
 */
@NullMarked
public class AnimationHandler {

    private @Nullable Animator mCurrentAnimator;

    /** Forces the completion of a possibly incomplete instance of the contained animation. */
    public void forceFinishAnimation() {
        if (mCurrentAnimator != null) {
            mCurrentAnimator.end();
            assert mCurrentAnimator == null;
        }
    }

    /**
     * Starts the animation. Ensures that the previous instance of the animation is complete prior
     * to starting said animation.
     */
    public void startAnimation(Animator animation) {
        if (mCurrentAnimator != null) {
            forceFinishAnimation();
        }

        assert mCurrentAnimator == null;
        mCurrentAnimator = animation;

        clearCurrentAnimatorOnAnimationEnd();
        mCurrentAnimator.start();
    }

    /** Add a listener to the animator to help keep track of the animation is complete. */
    private void clearCurrentAnimatorOnAnimationEnd() {
        assert mCurrentAnimator != null;
        mCurrentAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        assert Objects.equals(animation, mCurrentAnimator);
                        mCurrentAnimator = null;
                    }
                });
    }

    /** Checks if the animation is present. */
    public boolean isAnimationPresent() {
        return mCurrentAnimator != null;
    }
}

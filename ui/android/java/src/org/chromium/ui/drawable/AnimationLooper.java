// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.drawable;

import android.animation.ValueAnimator;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.ResettersForTesting;

/**
 * Encapsulates the logic to loop animated drawables from both Android Framework. The animation
 * should be started and stopped using {@link #start()} and {@link #stop()}.
 */
public class AnimationLooper {
    private static @Nullable Boolean sAreAnimatorsEnabledForTests;

    private final Handler mHandler;
    private final Animatable mAnimatable;
    private final Animatable2Compat.AnimationCallback mAnimationCallback;

    private boolean mIsRunning;

    /**
     * @param drawable The drawable should be drawable and animatable at the same time.
     */
    public AnimationLooper(Drawable drawable) {
        mHandler = new Handler();
        mAnimatable = (Animatable) drawable;
        mAnimationCallback =
                new Animatable2Compat.AnimationCallback() {
                    @Override
                    public void onAnimationEnd(Drawable drawable) {
                        mHandler.post(mAnimatable::start);
                    }
                };
    }

    /** Starts the animation of the associated drawable. */
    public void start() {
        if (areAnimatorsEnabled()) {
            assert !mIsRunning : "Animation is already running!";
            AnimatedVectorDrawableCompat.registerAnimationCallback(
                    (Drawable) mAnimatable, mAnimationCallback);
            mAnimatable.start();
            mIsRunning = true;
        }
    }

    /** Stops the animation of the associated drawable. */
    public void stop() {
        if (mIsRunning) {
            AnimatedVectorDrawableCompat.unregisterAnimationCallback(
                    (Drawable) mAnimatable, mAnimationCallback);
            mAnimatable.stop();
            mIsRunning = false;
        }
    }

    private static boolean areAnimatorsEnabled() {
        if (sAreAnimatorsEnabledForTests != null) return sAreAnimatorsEnabledForTests;

        return ValueAnimator.areAnimatorsEnabled();
    }

    static void setAreAnimatorsEnabledForTests(@Nullable Boolean areAnimatorsEnabled) {
        sAreAnimatorsEnabledForTests = areAnimatorsEnabled;
        ResettersForTesting.register(() -> sAreAnimatorsEnabledForTests = null);
    }
}

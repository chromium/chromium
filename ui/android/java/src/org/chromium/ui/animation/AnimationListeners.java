// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

/**
 * A utility class for creating {@link AnimatorListener} instances from callbacks. This helps avoid
 * the boilerplate of creating an anonymous {@link AnimatorListenerAdapter} when you only care about
 * a single animation event.
 */
@NullMarked
public class AnimationListeners {
    /**
     * Creates a listener that executes a callback when an animation starts.
     *
     * @param callback Invoked when the animation starts. The animator that started is passed as an
     *     argument.
     * @return An {@link AnimatorListener} that listens for the start event.
     */
    public static AnimatorListener onAnimationStart(Callback<Animator> callback) {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                callback.onResult(animation);
            }
        };
    }

    /**
     * Creates a listener that executes a callback when an animation ends.
     *
     * @param callback Invoked when the animation ends. The animator that ended is passed as an
     *     argument.
     * @return An {@link AnimatorListener} that listens for the end event.
     */
    public static AnimatorListener onAnimationEnd(Callback<Animator> callback) {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                callback.onResult(animation);
            }
        };
    }

    /**
     * Creates a listener that executes a callback when an animation is canceled.
     *
     * @param callback Invoked when the animation is canceled. The animator that was canceled is
     *     passed as an argument.
     * @return An {@link AnimatorListener} that listens for the cancel event.
     */
    public static AnimatorListener onAnimationCancel(Callback<Animator> callback) {
        return new AnimatorListenerAdapter() {
            @Override
            public void onAnimationCancel(Animator animation) {
                callback.onResult(animation);
            }
        };
    }
}

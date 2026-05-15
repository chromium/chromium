// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation.transition;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.transition.Transition;
import android.transition.TransitionValues;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This {@link Transition} animates between two integer values, notifying the callback on each
 * update. Each invocation will be a new value, with repeated updates being filtered out.
 */
@NullMarked
public class IntegerValueTransition extends Transition {
    // This is just a placeholder property to facilitate the Transition framework picking up and
    // including this Transition. If no properties are registered to have changed via
    // #captureStartValues() and #captureEndValues(), the Transition will be discarded.
    private static final String PROPERTY_PROGRESS = "IntegerValueTransition:progress";

    private final int mStartValue;
    private final int mTargetValue;
    private final Callback<Integer> mOnUpdate;

    /**
     * Creates an IntegerValueTransition that animates between two integer values.
     *
     * @param view A view used as a target for the Transition. This view is just used to restrict
     *     the Transition to a single instance, the specific view that is chosen does not matter.
     * @param startValue The starting value of the animation.
     * @param targetValue The target value of the animation.
     * @param onUpdate The callback to be notified on each update with the current value.
     */
    public IntegerValueTransition(
            View view, int startValue, int targetValue, Callback<Integer> onUpdate) {
        super.addTarget(view);
        mStartValue = startValue;
        mTargetValue = targetValue;
        mOnUpdate = onUpdate;
    }

    @Override
    public void captureStartValues(TransitionValues transitionValues) {
        // Placeholder property change to ensure this Transition is not discarded.
        transitionValues.values.put(PROPERTY_PROGRESS, 0f);
    }

    @Override
    public void captureEndValues(TransitionValues transitionValues) {
        // Placeholder property change to ensure this Transition is not discarded.
        transitionValues.values.put(PROPERTY_PROGRESS, 1f);
    }

    @Nullable
    @Override
    public Animator createAnimator(
            ViewGroup sceneRoot,
            @Nullable TransitionValues startValues,
            @Nullable TransitionValues endValues) {
        ValueAnimator valueAnimator = new ValueAnimator();
        valueAnimator.setIntValues(mStartValue, mTargetValue);
        valueAnimator.addUpdateListener(
                animator -> mOnUpdate.onResult((int) animator.getAnimatedValue()));
        return valueAnimator;
    }

    @Override
    public Transition addTarget(View target) {
        assert false
                : "#addTarget() should not be used with this Transition, as it would lead to"
                      + " duplicate Animators. All logic for this Transition should be kept in the"
                      + " input callback.";
        return super.addTarget(target);
    }

    @Override
    public Transition addTarget(int targetId) {
        assert false
                : "#addTarget() should not be used with this Transition, as it would lead to"
                      + " duplicate Animators. All logic for this Transition should be kept in the"
                      + " input callback.";
        return super.addTarget(targetId);
    }

    @Override
    public Transition addTarget(Class targetType) {
        assert false
                : "#addTarget() should not be used with this Transition, as it would lead to"
                      + " duplicate Animators. All logic for this Transition should be kept in the"
                      + " input callback.";
        return super.addTarget(targetType);
    }

    @Override
    public Transition addTarget(String targetName) {
        assert false
                : "#addTarget() should not be used with this Transition, as it would lead to"
                      + " duplicate Animators. All logic for this Transition should be kept in the"
                      + " input callback.";
        return super.addTarget(targetName);
    }
}

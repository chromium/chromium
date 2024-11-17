// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.drawable;

import android.content.Context;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.AnimatedStateListDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;

import java.util.ArrayList;
import java.util.List;

/**
 * Helper that simplifies StateListDrawable and AnimatedStateListDrawable creation. Stateful
 * drawables have to be created in Java for now, as drawables specified in XML can't reference
 * vector drawables on platform versions where VectorDrawableCompat is used (API level 23 and
 * below).
 *
 * {@link #build()} will instantiate AnimatedStateListDrawable on platforms where it is supported
 * (API level 21+). On older APIs, transition animations will be ignored and StateListDrawable will
 * be instantiated instead.
 *
 * Usage:
 * StateListDrawableBuilder builder = new StateListDrawableBuilder(context);
 * StateListDrawableBuilder.State checked =
 *         builder.addState(R.drawable.checked, android.R.attr.state_checked);
 * StateListDrawableBuilder.State unchecked = builder.addState(R.drawable.unchecked);
 * builder.addTransition(checked, unchecked, R.drawable.transition_checked_unchecked);
 * builder.addTransition(unchecked, checked, R.drawable.transition_unchecked_checked);
 * StateListDrawable drawable = builder.build();
 */
public class StateListDrawableBuilder {
    /** Identifies single state of the drawable. Used by {@link #addTransition}. */
    public static class State {
        private final @DrawableRes int mDrawable;
        private final int[] mStateSet;
        private final int mStateId;

        private State(@DrawableRes int drawable, int[] stateSet, int stateId) {
            mDrawable = drawable;
            mStateSet = stateSet;
            mStateId = stateId;
        }

        private @DrawableRes int getDrawable() {
            return mDrawable;
        }

        private int[] getStateSet() {
            return mStateSet;
        }

        private int getStateId() {
            return mStateId;
        }
    }

    private static class Transition {
        private final @DrawableRes int mDrawable;
        private final int mFromStateId;
        private final int mToStateId;

        private Transition(@DrawableRes int drawable, int fromStateId, int toStateId) {
            mDrawable = drawable;
            mFromStateId = fromStateId;
            mToStateId = toStateId;
        }

        private @DrawableRes int getDrawable() {
            return mDrawable;
        }

        private int getFromId() {
            return mFromStateId;
        }

        private int getToId() {
            return mToStateId;
        }
    }

    private final Context mContext;
    private final List<State> mStates = new ArrayList<>();
    private final List<Transition> mTransitions = new ArrayList<>();

    public StateListDrawableBuilder(Context context) {
        mContext = context;
    }

    /**
     * Add state to the drawable. Please note that order of calls to this method is important, as
     * StateListDrawable will pick the first state which stateSet matches View state.
     * @param drawable Id of the drawable for the added state. May refer to a vector drawable.
     * @param stateSet Array of state ids that specify the state. See {@link android.R.attr}
     *         for the list of state ids provided by the platform.
     */
    public State addState(@DrawableRes int drawable, int... stateSet) {
        int nextStateId = mStates.size() + 1; // State ids should be greater than 1.
        State state = new State(drawable, stateSet, nextStateId);
        mStates.add(state);
        return state;
    }

    /**
     * Add transition animation to the stateful drawable.
     * @param from The state of the stateful drawable before the transition.
     * @param to The state of the stateful drawable after the transition.
     * @param drawable Id of the animated drawable for the transition. Must refer to animated vector
     *         drawable.
     */
    public void addTransition(State from, State to, @DrawableRes int drawable) {
        assert mStates.contains(from) && mStates.contains(to) : "State from a different builder!";
        Transition transition = new Transition(drawable, from.getStateId(), to.getStateId());
        mTransitions.add(transition);
    }

    /**
     * Build drawable from added states and transitions.
     * @return AnimatedStateListDrawable if platform supports it, StateListDrawable otherwise.
     */
    public StateListDrawable build() {
        AnimatedStateListDrawable result = new AnimatedStateListDrawable();
        int statesSize = mStates.size();
        for (int i = 0; i < statesSize; ++i) {
            State state = mStates.get(i);
            Drawable drawable = AppCompatResources.getDrawable(mContext, state.getDrawable());
            assert drawable != null;
            result.addState(state.getStateSet(), drawable, state.getStateId());
        }
        int transitionsSize = mTransitions.size();
        for (int i = 0; i < transitionsSize; ++i) {
            Transition transition = mTransitions.get(i);
            Drawable drawable = AppCompatResources.getDrawable(mContext, transition.getDrawable());
            result.addTransition(
                    transition.getFromId(),
                    transition.getToId(),
                    (Drawable & Animatable) drawable,
                    false);
        }
        return result;
    }
}

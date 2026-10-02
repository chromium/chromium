// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation.transition;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.view.View;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ShrinkTransition}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ShrinkTransitionUnitTest {
    private static final float DELTA = 0.0001f;

    private final View mView = new View(ContextUtils.getApplicationContext());

    @Test
    public void testOnAppear() {
        float goneScale = 0.6f;
        float visibleScale = 1.0f;
        ShrinkTransition transition = new ShrinkTransition(goneScale, visibleScale);
        Animator animator = transition.onAppear(null, mView, null, null);
        assertNotNull(animator);

        animator.start();
        assertEquals(goneScale, mView.getScaleX(), DELTA);
        assertEquals(goneScale, mView.getScaleY(), DELTA);

        ShadowLooper.runUiThreadTasks();
        assertEquals(visibleScale, mView.getScaleX(), DELTA);
        assertEquals(visibleScale, mView.getScaleY(), DELTA);
    }

    @Test
    public void testOnDisappear() {
        float goneScale = 0.6f;
        float visibleScale = 1.0f;
        ShrinkTransition transition = new ShrinkTransition(goneScale, visibleScale);
        ValueAnimator animator = (ValueAnimator) transition.onDisappear(null, mView, null, null);
        assertNotNull(animator);

        mView.setScaleX(0.8f);
        mView.setScaleY(0.8f);
        animator.start();
        assertEquals(visibleScale, mView.getScaleX(), DELTA);
        assertEquals(visibleScale, mView.getScaleY(), DELTA);

        // Jump to the end of the animation, before it has finished.
        animator.setCurrentFraction(1f);
        assertEquals(goneScale, mView.getScaleX(), DELTA);
        assertEquals(goneScale, mView.getScaleY(), DELTA);

        // The scale is reset to visibleScale in onAnimationEnd.
        ShadowLooper.runUiThreadTasks();
        assertEquals(visibleScale, mView.getScaleX(), DELTA);
        assertEquals(visibleScale, mView.getScaleY(), DELTA);
    }
}

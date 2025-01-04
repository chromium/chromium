// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.animation.Animator;
import android.animation.ValueAnimator;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AnimationHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AnimationHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AnimationHandler mHandler;
    private @Mock Animator mAnimator;

    @Before
    public void setUp() {
        mHandler = new AnimationHandler();
    }

    @Test
    public void testStartAnimation() {
        mHandler.startAnimation(mAnimator);
        verify(mAnimator).start();
        verify(mAnimator).addListener(any());
    }

    @Test
    public void testAnimationPresent() {
        mHandler.startAnimation(mAnimator);
        assertTrue(mHandler.isAnimationPresent());
    }

    @Test
    public void testNoAnimationPresent() {
        assertFalse(mHandler.isAnimationPresent());
    }

    @Test
    public void testForceAnimation() {
        Animator animator = ValueAnimator.ofFloat(0, 1);
        animator.setDuration(Long.MAX_VALUE);

        mHandler.startAnimation(animator);
        assertTrue(mHandler.isAnimationPresent());

        mHandler.forceFinishAnimation();
        assertFalse(mHandler.isAnimationPresent());
    }

    @Test(expected = AssertionError.class)
    public void testForceAnimationFails() {
        mHandler.startAnimation(mAnimator);
        mHandler.forceFinishAnimation();
    }
}

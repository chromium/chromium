// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.view.View;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.animation.PathAnimationUtils.ArcDirection;

/** Unit tests for {@link TranslationAnimatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CommonAnimationsFactoryUnitTest {
    private static final long DURATION_MS = 10L;
    private static final float DELTA = 0.0001f;

    private final View mView = new View(ContextUtils.getApplicationContext());

    @Test
    public void testFadeIn() {
        mView.setVisibility(View.GONE);

        Animator animator = CommonAnimationsFactory.createFadeInAnimation(mView);
        animator.setDuration(DURATION_MS);
        animator.start();

        assertEquals(0f, mView.getAlpha(), DELTA);
        assertEquals(View.VISIBLE, mView.getVisibility());

        ShadowLooper.runUiThreadTasks();
        assertEquals(1f, mView.getAlpha(), DELTA);
    }

    @Test
    public void testFadeOut() {
        mView.setAlpha(0.5f);

        ValueAnimator animator =
                (ValueAnimator) CommonAnimationsFactory.createFadeOutAnimation(mView);
        animator.setDuration(DURATION_MS);
        animator.start();
        assertEquals(1f, mView.getAlpha(), DELTA);

        animator.setCurrentFraction(0.5f);
        float midAlpha = mView.getAlpha();
        assertTrue(midAlpha > 0f && midAlpha < 1f);

        animator.setCurrentFraction(1f);
        assertEquals(0f, mView.getAlpha(), DELTA);
        assertEquals(View.VISIBLE, mView.getVisibility());

        ShadowLooper.runUiThreadTasks();
        // Alpha is restored once the view is hidden.
        assertEquals(1f, mView.getAlpha(), DELTA);
        assertEquals(View.GONE, mView.getVisibility());
    }

    @Test
    public void testCreateViewArcAnimation() {
        Animator animator =
                CommonAnimationsFactory.createViewArcAnimation(
                        mView, 10f, 20f, 50f, 80f, ArcDirection.CLOCKWISE);
        assertTrue(animator instanceof ObjectAnimator);
        assertEquals(mView, ((ObjectAnimator) animator).getTarget());
    }
}

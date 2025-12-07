// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TranslationAnimatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class DrawableTranslationAnimatorFactoryUnitTest {
    private static final long DURATION_MS = 10L;
    private static final int[] NUMBERS_LIST = new int[] {321, 4142, 2311, 23, 9456, 65464};
    private static final int[] COEFFICIENTS = new int[] {-1, 1};
    private static final int DRAWABLE_WIDTH = 10;
    private static final int DRAWABLE_HEIGHT = 20;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Drawable mDrawable;

    @FunctionalInterface
    interface DrawableTranslationAnimatorFactoryTestMethod {
        void run(int x, int y);
    }

    @Before
    public void setUp() {
        // Ensures that fresh bounds are being created for each method call.
        when(mDrawable.copyBounds()).thenAnswer(answer -> initDrawableBounds());

        when(mDrawable.getIntrinsicHeight()).thenReturn(DRAWABLE_HEIGHT);
        when(mDrawable.getIntrinsicWidth()).thenReturn(DRAWABLE_WIDTH);
    }

    @Test
    public void testNoDisplacement() {
        Rect bounds = initDrawableBounds();
        Animator animator = DrawableTranslationAnimatorFactory.build(mDrawable, 0, 0);
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mDrawable, atLeastOnce()).setBounds(bounds);
        verify(mDrawable, never())
                .setBounds(
                        argThat(
                                rect ->
                                        rect.top != bounds.top
                                                || rect.bottom != bounds.bottom
                                                || rect.left != bounds.left
                                                || rect.right != bounds.right));
    }

    @Test
    public void testReachesDisplacement() {
        runTestMethodForAllNumbers(this::checkReachesDisplacement);
    }

    @Test
    public void testReturnsToNoDisplacement() {
        runTestMethodForAllNumbers(this::checkReturnsToNoDisplacement);
    }

    private void checkReturnsToNoDisplacement(int expectedDx, int expectedDy) {
        ArgumentCaptor<Rect> captor = ArgumentCaptor.forClass(Rect.class);

        Rect endRect = initDrawableBounds();
        Animator animator =
                DrawableTranslationAnimatorFactory.build(mDrawable, expectedDx, expectedDy);
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mDrawable, atLeastOnce()).copyBounds();
        verify(mDrawable, atLeastOnce()).setBounds(captor.capture());
        assertEquals(
                "Failed to return initial displacement for ("
                        + expectedDx
                        + ", "
                        + expectedDy
                        + ")",
                endRect,
                captor.getValue());
    }

    private void checkReachesDisplacement(int expectedDx, int expectedDy) {
        Rect origin = initDrawableBounds();
        Rect fullyDisplaced =
                new Rect(
                        origin.left + expectedDx,
                        origin.top + expectedDy,
                        origin.right + expectedDx,
                        origin.bottom + expectedDy);
        Animator animator =
                DrawableTranslationAnimatorFactory.build(mDrawable, expectedDx, expectedDy);
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mDrawable, atLeastOnce()).copyBounds();
        // Bounds can sometimes be set twice depending on the displacement.
        verify(mDrawable, atMost(2)).setBounds(fullyDisplaced);
    }

    private void runTestMethodForAllNumbers(DrawableTranslationAnimatorFactoryTestMethod method) {
        for (int x : NUMBERS_LIST) {
            for (int y : NUMBERS_LIST) {
                for (int xCoefficient : COEFFICIENTS) {
                    for (int yCoefficient : COEFFICIENTS) {
                        method.run(xCoefficient * x, yCoefficient * y);
                    }
                }
            }
        }
    }

    private Rect initDrawableBounds() {
        return new Rect(0, 0, DRAWABLE_WIDTH, DRAWABLE_HEIGHT);
    }
}

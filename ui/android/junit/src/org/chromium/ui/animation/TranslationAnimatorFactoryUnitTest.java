// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertTrue;

import android.animation.Animator;
import android.os.Looper;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link TranslationAnimatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class TranslationAnimatorFactoryUnitTest {
    private static final long DURATION_MS = 3000L;
    private static final int[] NUMBERS_LIST = new int[] {321, 4142, 2311, 23, 0, 9456, 65464};
    private static final int[] COEFFICIENTS = new int[] {-1, 1};
    private static final float PERMITTED_DELTA = 0.01f;

    @FunctionalInterface
    interface TranslationAnimatorFactoryTestMethod {
        void run(int x, int y);
    }

    @Test
    public void testVerticalTranslationFollowsPath() {
        for (int expectedDy : NUMBERS_LIST) {
            for (int coefficient : COEFFICIENTS) {
                checkVerticalTranslationFollowsPath(coefficient * expectedDy);
            }
        }
    }

    @Test
    public void testReachesDisplacement() {
        runTestMethodForAllNumbers(this::checkReachesDisplacement);
    }

    @Test
    public void testTranslationFollowsPath() {
        runTestMethodForAllNumbers(this::checkTranslationFollowsPath);
    }

    @Test
    public void testReturnsToNoDisplacement() {
        runTestMethodForAllNumbers(this::checkReturnsToNoDisplacement);
    }

    private void checkTranslationFollowsPath(int expectedDx, int expectedDy) {
        // Line equation: y = gradient * x, where gradient = expectedDy / expectedDx

        if (expectedDx == 0) {
            // Don't run this for vertical lines.
            return;
        }

        float gradient = ((float) expectedDy) / ((float) expectedDx);

        AtomicBoolean followsPath = new AtomicBoolean(true);
        AtomicBoolean calledOnce = new AtomicBoolean(false);
        Animator animator =
                TranslationAnimatorFactory.buildTranslationAnimation(
                        expectedDx,
                        expectedDy,
                        (x, y) -> {
                            boolean approxEq =
                                    areApproximatelyEqual(y, x * gradient, PERMITTED_DELTA);
                            followsPath.set(followsPath.get() && approxEq);
                            calledOnce.set(true);
                        });
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertTrue(
                "Failed to follow path for (" + expectedDx + ", " + expectedDy + ")",
                followsPath.get());
        assertTrue(
                "Animator failed be called for (" + expectedDx + ", " + expectedDy + ")",
                calledOnce.get());
    }

    private void checkVerticalTranslationFollowsPath(int expectedDy) {
        AtomicBoolean followsPath = new AtomicBoolean(true);
        AtomicBoolean calledOnce = new AtomicBoolean(false);
        Animator animator =
                TranslationAnimatorFactory.buildTranslationAnimation(
                        0,
                        expectedDy,
                        (x, y) -> {
                            followsPath.set(
                                    followsPath.get()
                                            && areApproximatelyEqual(x, 0, PERMITTED_DELTA));
                            calledOnce.set(true);
                        });
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertTrue(
                "Failed to follow path for vertical translation of " + expectedDy + ")",
                followsPath.get());
        assertTrue(
                "Animator failed be called for vertical translation of " + expectedDy + ")",
                calledOnce.get());
    }

    private void checkReturnsToNoDisplacement(int expectedDx, int expectedDy) {
        AtomicBoolean reachesDisplacement = new AtomicBoolean(false);
        AtomicBoolean returnsToOrigin = new AtomicBoolean(false);
        Animator animator =
                TranslationAnimatorFactory.buildTranslationAnimation(
                        expectedDx,
                        expectedDy,
                        (x, y) -> {
                            reachesDisplacement.set(
                                    reachesDisplacement.get()
                                            || reachesDisplacement(
                                                    x, y, expectedDx, expectedDy, PERMITTED_DELTA));
                            // Will fail if not set to true at very last onUpdate() call, which is
                            // what we want.
                            returnsToOrigin.set(reachesDisplacement.get() && reachesOrigin(x, y));
                        });
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertTrue(
                "Failed to return to initial displacement for ("
                        + expectedDx
                        + ", "
                        + expectedDy
                        + ")",
                reachesDisplacement.get());
    }

    private void checkReachesDisplacement(int expectedDx, int expectedDy) {
        AtomicBoolean reachesDisplacement = new AtomicBoolean(false);
        Animator animator =
                TranslationAnimatorFactory.buildTranslationAnimation(
                        expectedDx,
                        expectedDy,
                        (x, y) ->
                                reachesDisplacement.set(
                                        reachesDisplacement.get()
                                                || reachesDisplacement(
                                                        x,
                                                        y,
                                                        expectedDx,
                                                        expectedDy,
                                                        PERMITTED_DELTA)));
        animator.setDuration(DURATION_MS);
        animator.start();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertTrue(
                "Failed to reach maximum displacement for (" + expectedDx + ", " + expectedDy + ")",
                reachesDisplacement.get());
    }

    private void runTestMethodForAllNumbers(TranslationAnimatorFactoryTestMethod method) {
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

    private static boolean reachesOrigin(float x, float y) {
        return x == 0.f && y == 0.f;
    }

    private static boolean reachesDisplacement(
            float x, float y, float expectedDx, float expectedDy, float delta) {
        return areApproximatelyEqual(x, expectedDx, delta)
                && areApproximatelyEqual(y, expectedDy, delta);
    }

    private static boolean areApproximatelyEqual(float a, float b, float delta) {
        return (a - b) < delta;
    }
}

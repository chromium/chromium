// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.animation.ValueAnimator;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.Locale;

/** Utilities related to render testing for animations. */
public class RenderTestAnimationUtils {
    /**
     * Steps through an animation and generates rendered images for each step.
     *
     * @param testcaseName The base name for the render test results.
     * @param renderTestRule The {@link RenderTestRule} for the render test.
     * @param rootView The root {@link View} where the render test takes place.
     * @param animator The {@link ValueAnimator} for the render test.
     * @param steps The number of steps the animation should take (2 or more).
     * @throws IOException if the rendered image cannot be saved to the device.
     */
    public static void stepThroughAnimation(
            String testcaseName,
            RenderTestRule renderTestRule,
            View rootView,
            ValueAnimator animator,
            int steps)
            throws IOException {
        assert steps >= 2;
        float fractionPerStep = 1.0f / (steps - 1);

        // Manually drive the animation instead of using a ValueAnimator for exact control over
        // step size and timing.
        for (int step = 0; step < steps; step++) {
            final float animationFraction = fractionPerStep * step;
            ThreadUtils.runOnUiThreadBlocking(() -> animator.setCurrentFraction(animationFraction));

            renderTestRule.render(
                    rootView,
                    testcaseName
                            + String.format(Locale.ENGLISH, "_step_%d_of_%d", step + 1, steps));
        }
    }
}

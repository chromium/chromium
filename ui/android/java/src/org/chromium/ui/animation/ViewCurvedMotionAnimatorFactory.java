// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.animation.ObjectAnimator;
import android.graphics.Path;
import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** Factory class for creating a curved movement in {@link View}s. */
@NullMarked
public class ViewCurvedMotionAnimatorFactory {
    /**
     * Builds an animator to translate a view with a curved motion.
     *
     * @param view The view to move.
     * @param startPoint The coordinates for the start point.
     * @param endPoint The coordinates for the end point.
     * @param isClockwise Whether the Path arc direction is clockwise.
     * @return The {@link ObjectAnimator} for the curved motion.
     */
    public static ObjectAnimator build(
            View view, float[] startPoint, float[] endPoint, boolean isClockwise) {
        Path path = new Path();
        PathAnimationUtils.addArcToPathForArcAnimation(startPoint, endPoint, isClockwise, path);
        return ObjectAnimator.ofFloat(view, View.X, View.Y, path);
    }

    /**
     * Builds an animator to translate a view with a curved motion.
     *
     * @param view The view to move.
     * @param x1 The x-coordinate for the start point.
     * @param y1 The y-coordinate for the start point.
     * @param x2 The x-coordinate for the end point.
     * @param y2 The y-coordinate for the end point.
     * @param isClockwise Whether the Path arc direction is clockwise.
     * @return The {@link ObjectAnimator} for the curved motion.
     */
    public static ObjectAnimator build(
            View view, float x1, float y1, float x2, float y2, boolean isClockwise) {
        return build(view, new float[] {x1, y1}, new float[] {x2, y2}, isClockwise);
    }
}

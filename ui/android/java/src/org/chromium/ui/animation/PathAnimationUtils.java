// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.graphics.RectF;

import org.chromium.build.annotations.NullMarked;

/** Utilities related to {@link android.graphics.Path} for animations */
@NullMarked
public class PathAnimationUtils {
    /**
     * Returns a RectF which represents the oval to perform Path#arcTo movements.
     *
     * @param startPoint The coordinates for the start point.
     * @param endPoint The coordinates for the end point.
     * @param isClockwise Whether the Path arc direction is clockwise.
     * @return The {@link RectF} that represents the oval.
     */
    public static RectF createRectForArcAnimation(
            float[] startPoint, float[] endPoint, boolean isClockwise) {
        float startX = startPoint[0];
        float startY = startPoint[1];
        float endX = endPoint[0];
        float endY = endPoint[1];

        assert startX != endX;
        assert startY != endY;

        float horizontalRadius = Math.abs(startX - endX);
        float verticalRadius = Math.abs(startY - endY);
        float centerX;
        float centerY;

        if ((!isClockwise && ((startX > endX) == (startY > endY)))
                || (isClockwise && ((startX > endX) != (startY > endY)))) {
            centerX = endX;
            centerY = startY;
        } else {
            centerX = startX;
            centerY = endY;
        }

        return new RectF(
                centerX - horizontalRadius,
                centerY - verticalRadius,
                centerX + horizontalRadius,
                centerY + verticalRadius);
    }
}

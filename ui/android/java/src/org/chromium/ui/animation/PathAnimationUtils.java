// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.graphics.Path;
import android.graphics.RectF;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;

/** Utilities related to {@link Path} for animations */
@NullMarked
public class PathAnimationUtils {
    private static final int START_ANGLE_RIGHT = 0;
    private static final int START_ANGLE_BOTTOM = 90;
    private static final int START_ANGLE_LEFT = 180;
    private static final int START_ANGLE_TOP = 270;

    /**
     * Creates a {@link RectF} which represents the oval to perform a {@link Path#arcTo} movement
     * between two points.
     *
     * @param startPoint The coordinates for the start point.
     * @param endPoint The coordinates for the end point.
     * @param isClockwise Whether the {@link Path} arc direction is clockwise.
     * @return The {@link RectF} that represents the oval.
     */
    public static RectF createRectForArcAnimation(
            float[] startPoint, float[] endPoint, boolean isClockwise) {
        assertPointsForArcAnimation(startPoint, endPoint);

        float startX = startPoint[0];
        float startY = startPoint[1];
        float endX = endPoint[0];
        float endY = endPoint[1];

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

    /**
     * Selects the proper start angle to perform a {@link Path#arcTo} movement between two points.
     *
     * @param startPoint The coordinates for the start point.
     * @param endPoint The coordinates for the end point.
     * @param isClockwise Whether the {@link Path} arc direction is clockwise.
     * @return The start angle value
     */
    public static int getStartAngleForArcAnimation(
            float[] startPoint, float[] endPoint, boolean isClockwise) {
        assertPointsForArcAnimation(startPoint, endPoint);

        float startX = startPoint[0];
        float startY = startPoint[1];
        float endX = endPoint[0];
        float endY = endPoint[1];

        if (startX > endX && (isClockwise ^ (startY > endY))) {
            return START_ANGLE_RIGHT;
        } else if (startY > endY && (isClockwise ^ (startX < endX))) {
            return START_ANGLE_BOTTOM;
        } else if (startX < endX && (isClockwise ^ (startY < endY))) {
            return START_ANGLE_LEFT;
        } else {
            return START_ANGLE_TOP;
        }
    }

    /**
     * Add a {@link Path#arcTo} movement between two points to a given Path object.
     *
     * @param startPoint The coordinates for the start point.
     * @param endPoint The coordinates for the end point.
     * @param isClockwise Whether the {@link Path} arc direction is clockwise.
     */
    public static void addArcToPathForArcAnimation(
            float[] startPoint, float[] endPoint, boolean isClockwise, Path path) {
        RectF oval = createRectForArcAnimation(startPoint, endPoint, isClockwise);
        int startAngle = getStartAngleForArcAnimation(startPoint, endPoint, isClockwise);
        int sweepAngle = isClockwise ? 90 : -90;
        path.arcTo(oval, startAngle, sweepAngle);
    }

    /**
     * Asserts both points are valid for a {@link Path#arcTo} movement between two points.
     *
     * @param firstPoint The coordinates for the start point.
     * @param secondPoint The coordinates for the end point.
     */
    @VisibleForTesting
    static void assertPointsForArcAnimation(float[] firstPoint, float[] secondPoint) {
        assert firstPoint.length == 2 && secondPoint.length == 2
                : "Each point should only contain x and y (length of 2)";
        assert firstPoint[0] != secondPoint[0] && firstPoint[1] != secondPoint[1]
                : "Coordinates x and y should be different for both points.";
    }
}

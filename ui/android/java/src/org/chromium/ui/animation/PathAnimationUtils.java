// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.animation.ObjectAnimator;
import android.graphics.Path;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Utilities related to {@link Path} and arc animations. */
@NullMarked
public class PathAnimationUtils {
    private PathAnimationUtils() {}

    @IntDef({ArcDirection.CLOCKWISE, ArcDirection.COUNTER_CLOCKWISE})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface ArcDirection {
        int CLOCKWISE = 0;
        int COUNTER_CLOCKWISE = 1;
    }

    private static final int START_ANGLE_RIGHT = 0;
    private static final int START_ANGLE_BOTTOM = 90;
    private static final int START_ANGLE_LEFT = 180;
    private static final int START_ANGLE_TOP = 270;

    /**
     * Creates an {@link ObjectAnimator} to translate a {@link View} along an arc path.
     *
     * @param view The view to animate.
     * @param startX Starting X coordinate.
     * @param startY Starting Y coordinate.
     * @param endX Ending X coordinate.
     * @param endY Ending Y coordinate.
     * @param direction The {@link ArcDirection} (clockwise or counter-clockwise).
     * @return An {@link ObjectAnimator} animating {@link View#X} and {@link View#Y}.
     */
    public static ObjectAnimator createViewArcAnimator(
            View view,
            float startX,
            float startY,
            float endX,
            float endY,
            @ArcDirection int direction) {
        Path path = createArcPath(startX, startY, endX, endY, direction);
        return ObjectAnimator.ofFloat(view, View.X, View.Y, path);
    }

    /**
     * Creates a {@link Path} containing an arc or straight movement between two points.
     *
     * @param startX Starting X coordinate.
     * @param startY Starting Y coordinate.
     * @param endX Ending X coordinate.
     * @param endY Ending Y coordinate.
     * @param direction The {@link ArcDirection} (clockwise or counter-clockwise).
     * @return The {@link Path} with the arc or linear contour.
     */
    public static Path createArcPath(
            float startX, float startY, float endX, float endY, @ArcDirection int direction) {
        Path path = new Path();
        if (MathUtils.areFloatsEqual(startX, endX) || MathUtils.areFloatsEqual(startY, endY)) {
            path.moveTo(startX, startY);
            path.lineTo(endX, endY);
        } else {
            addArcToPath(path, startX, startY, endX, endY, direction);
        }
        return path;
    }

    /**
     * Appends an arc movement between two points to a given {@link Path}.
     *
     * @param path The {@link Path} to append the arc to.
     * @param startX Starting X coordinate.
     * @param startY Starting Y coordinate.
     * @param endX Ending X coordinate.
     * @param endY Ending Y coordinate.
     * @param direction The {@link ArcDirection} (clockwise or counter-clockwise).
     */
    @VisibleForTesting
    static void addArcToPath(
            Path path,
            float startX,
            float startY,
            float endX,
            float endY,
            @ArcDirection int direction) {
        assert !MathUtils.areFloatsEqual(startX, endX) && !MathUtils.areFloatsEqual(startY, endY)
                : "Coordinates should differ in both dimensions for an arc path.";
        boolean isClockwise = (direction == ArcDirection.CLOCKWISE);
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

        int startAngle = getStartAngleForArc(startX, startY, endX, endY, direction);
        int sweepAngle = isClockwise ? 90 : -90;

        path.arcTo(
                centerX - horizontalRadius,
                centerY - verticalRadius,
                centerX + horizontalRadius,
                centerY + verticalRadius,
                startAngle,
                sweepAngle,
                /* forceMoveTo= */ true);
    }

    /**
     * Selects the proper start angle to perform a {@link Path#arcTo} movement between two points.
     *
     * @param startX Starting X coordinate.
     * @param startY Starting Y coordinate.
     * @param endX Ending X coordinate.
     * @param endY Ending Y coordinate.
     * @param direction The {@link ArcDirection} (clockwise or counter-clockwise).
     * @return The start angle value.
     */
    @VisibleForTesting
    static int getStartAngleForArc(
            float startX, float startY, float endX, float endY, @ArcDirection int direction) {
        boolean isClockwise = (direction == ArcDirection.CLOCKWISE);
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
}

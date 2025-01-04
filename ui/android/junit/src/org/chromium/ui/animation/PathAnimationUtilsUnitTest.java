// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertEquals;

import android.graphics.RectF;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** JUnit tests for {@link PathAnimationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PathAnimationUtilsUnitTest {
    private static void assertRectF(RectF expected, RectF actual) {
        assertEquals(expected.left, actual.left, MathUtils.EPSILON);
        assertEquals(expected.top, actual.top, MathUtils.EPSILON);
        assertEquals(expected.right, actual.right, MathUtils.EPSILON);
        assertEquals(expected.bottom, actual.bottom, MathUtils.EPSILON);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantI_CounterClockwise() {
        float[] start = new float[] {72f, 50f};
        float[] end = new float[] {30f, 10f};

        RectF expectedRect = new RectF(-12f, 10f, 72f, 90f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, false);

        assertRectF(expectedRect, actualRect);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantI_Clockwise() {
        float[] start = new float[] {-20f, -14f};
        float[] end = new float[] {23f, 50f};

        RectF expectedRect = new RectF(-63f, -14f, 23f, 114f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, true);

        assertRectF(expectedRect, actualRect);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantII_CounterClockwise() {
        float[] start = new float[] {23f, -14f};
        float[] end = new float[] {-20f, 50f};

        RectF expectedRect = new RectF(-20f, -14f, 66f, 114f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, false);

        assertRectF(expectedRect, actualRect);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantII_Clockwise() {
        float[] start = new float[] {75f, 400f};
        float[] end = new float[] {120f, 10f};

        RectF expectedRect = new RectF(75f, 10f, 165f, 790f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, true);

        assertRectF(expectedRect, actualRect);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantIII_CounterClockwise() {
        float[] start = new float[] {-20f, -14f};
        float[] end = new float[] {622f, 50f};

        RectF expectedRect = new RectF(-20f, -78f, 1264f, 50f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, false);

        assertRectF(expectedRect, actualRect);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantIII_Clockwise() {
        float[] start = new float[] {740f, 200f};
        float[] end = new float[] {310f, 12f};

        RectF expectedRect = new RectF(310f, -176f, 1170f, 200f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, true);

        assertRectF(expectedRect, actualRect);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantIV_CounterClockwise() {
        float[] start = new float[] {20f, 100f};
        float[] end = new float[] {50f, 39f};

        RectF expectedRect = new RectF(-10f, -22f, 50f, 100f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, false);

        assertRectF(expectedRect, actualRect);
    }

    @Test
    public void testCreateRectForArcAnimation_QuadrantIV_Clockwise() {
        float[] start = new float[] {56.4f, 97.4f};
        float[] end = new float[] {-4.5f, 164f};

        RectF expectedRect = new RectF(-65.4f, 30.8f, 56.4f, 164f);
        RectF actualRect = PathAnimationUtils.createRectForArcAnimation(start, end, true);

        assertRectF(expectedRect, actualRect);
    }

    @Test(expected = AssertionError.class)
    public void testCreateRectForArcAnimation_InvalidInput_SamePoints() {
        float[] start = new float[] {6f, 6f};
        float[] end = new float[] {6f, 6f};

        PathAnimationUtils.createRectForArcAnimation(start, end, false);
    }

    @Test(expected = AssertionError.class)
    public void testCreateRectForArcAnimation_InvalidInput_SameX() {
        float[] start = new float[] {3.5f, 12f};
        float[] end = new float[] {3.5f, 43f};

        PathAnimationUtils.createRectForArcAnimation(start, end, false);
    }

    @Test(expected = AssertionError.class)
    public void testCreateRectForArcAnimation_InvalidInput_SameY() {
        float[] start = new float[] {12f, 3f};
        float[] end = new float[] {43f, 3f};

        PathAnimationUtils.createRectForArcAnimation(start, end, false);
    }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.graphics.Path;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.animation.PathAnimationUtils.ArcDirection;

/** JUnit tests for {@link PathAnimationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PathAnimationUtilsUnitTest {
    @Test
    public void testQuadrantI_CounterClockwise() {
        float startX = 72f;
        float startY = 50f;
        float endX = 30f;
        float endY = 10f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertEquals(0, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(
                path, startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertArcTo(path, -12f, 10f, 72f, 90f, 0, -90);
    }

    @Test
    public void testQuadrantI_Clockwise() {
        float startX = -20f;
        float startY = -14f;
        float endX = 23f;
        float endY = 50f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertEquals(270, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(path, startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertArcTo(path, -63f, -14f, 23f, 114f, 270, 90);
    }

    @Test
    public void testQuadrantII_CounterClockwise() {
        float startX = 23f;
        float startY = -14f;
        float endX = -20f;
        float endY = 50f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertEquals(270, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(
                path, startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertArcTo(path, -20f, -14f, 66f, 114f, 270, -90);
    }

    @Test
    public void testQuadrantII_Clockwise() {
        float startX = 75f;
        float startY = 400f;
        float endX = 120f;
        float endY = 10f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertEquals(180, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(path, startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertArcTo(path, 75f, 10f, 165f, 790f, 180, 90);
    }

    @Test
    public void testQuadrantIII_CounterClockwise() {
        float startX = -20f;
        float startY = -14f;
        float endX = 622f;
        float endY = 50f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertEquals(180, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(
                path, startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertArcTo(path, -20f, -78f, 1264f, 50f, 180, -90);
    }

    @Test
    public void testQuadrantIII_Clockwise() {
        float startX = 740f;
        float startY = 200f;
        float endX = 310f;
        float endY = 12f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertEquals(90, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(path, startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertArcTo(path, 310f, -176f, 1170f, 200f, 90, 90);
    }

    @Test
    public void testQuadrantIV_CounterClockwise() {
        float startX = 20f;
        float startY = 100f;
        float endX = 50f;
        float endY = 39f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertEquals(90, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(
                path, startX, startY, endX, endY, ArcDirection.COUNTER_CLOCKWISE);
        assertArcTo(path, -10f, -22f, 50f, 100f, 90, -90);
    }

    @Test
    public void testQuadrantIV_Clockwise() {
        float startX = 56.4f;
        float startY = 97.4f;
        float endX = -4.5f;
        float endY = 164f;

        int expectedAngle =
                PathAnimationUtils.getStartAngleForArc(
                        startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertEquals(0, expectedAngle);

        Path path = spy(new Path());
        PathAnimationUtils.addArcToPath(path, startX, startY, endX, endY, ArcDirection.CLOCKWISE);
        assertArcTo(path, -65.4f, 30.8f, 56.4f, 164f, 0, 90);
    }

    @Test
    public void testCreateArcPath_CollinearPoints_SameX() {
        Path path =
                PathAnimationUtils.createArcPath(
                        10f, 20f, 10f, 50f, ArcDirection.CLOCKWISE);
        // Should produce a valid non-empty straight line path without throwing assertion
        assertNotNull(path);
        assertFalse(path.isEmpty());
    }

    @Test
    public void testCreateArcPath_CollinearPoints_SameY() {
        Path path =
                PathAnimationUtils.createArcPath(
                        10f, 20f, 40f, 20f, ArcDirection.COUNTER_CLOCKWISE);
        // Should produce a valid non-empty straight line path without throwing assertion
        assertNotNull(path);
        assertFalse(path.isEmpty());
    }

    @Test
    public void testCreateArcPath_ArcPoints() {
        Path path =
                PathAnimationUtils.createArcPath(
                        10f, 20f, 50f, 80f, ArcDirection.CLOCKWISE);
        assertNotNull(path);
        assertFalse(path.isEmpty());
    }

    private static void assertArcTo(
            Path path,
            float expectedLeft,
            float expectedTop,
            float expectedRight,
            float expectedBottom,
            float expectedStartAngle,
            float expectedSweepAngle) {
        ArgumentCaptor<Float> left = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Float> top = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Float> right = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Float> bottom = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Float> startAngle = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Float> sweepAngle = ArgumentCaptor.forClass(Float.class);
        ArgumentCaptor<Boolean> forceMoveTo = ArgumentCaptor.forClass(Boolean.class);

        verify(path)
                .arcTo(
                        left.capture(),
                        top.capture(),
                        right.capture(),
                        bottom.capture(),
                        startAngle.capture(),
                        sweepAngle.capture(),
                        forceMoveTo.capture());

        assertEquals(expectedLeft, left.getValue(), MathUtils.EPSILON);
        assertEquals(expectedTop, top.getValue(), MathUtils.EPSILON);
        assertEquals(expectedRight, right.getValue(), MathUtils.EPSILON);
        assertEquals(expectedBottom, bottom.getValue(), MathUtils.EPSILON);
        assertEquals(expectedStartAngle, startAngle.getValue(), MathUtils.EPSILON);
        assertEquals(expectedSweepAngle, sweepAngle.getValue(), MathUtils.EPSILON);
        assertTrue(forceMoveTo.getValue());
    }
}

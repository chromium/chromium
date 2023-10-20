// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.dragdrop.AnimatedImageDragShadowBuilder.CursorOffset;
import org.chromium.ui.dragdrop.AnimatedImageDragShadowBuilder.DragShadowSpec;

/** Unit tests for {@link AnimatedImageDragShadowBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AnimatedImageDragShadowBuilderUnitTest {
    private static final int WINDOW_WIDTH = 1000;
    private static final int WINDOW_HEIGHT = 600;

    private final Context mContext = ApplicationProvider.getApplicationContext();

    @Test
    public void testResizeShadowImage_ScaleDownWithRatio() {
        doTestResizeShadowImage(
                "Resize 60%",
                /* width= */ 100,
                /* height= */ 100,
                /* targetWidth= */ 60,
                /* targetHeight= */ 60,
                /* startWidth= */ 100,
                /* startHeight= */ 100,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Resize 60%",
                /* width= */ 82,
                /* height= */ 82,
                /* targetWidth= */ 49,
                /* targetHeight= */ 49,
                /* startWidth= */ 82,
                /* startHeight= */ 82,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Resize 60%",
                /* width= */ 400,
                /* height= */ 400,
                /* targetWidth= */ 210,
                /* targetHeight= */ 210,
                /* startWidth= */ 400,
                /* startHeight= */ 400,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
    }

    @Test
    public void testResizeShadowImage_ScaleUpToRatio() {
        doTestResizeShadowImage(
                "Scale up to min size",
                /* width= */ 10,
                /* height= */ 10,
                /* targetWidth= */ 48,
                /* targetHeight= */ 48,
                /* startWidth= */ 10,
                /* startHeight= */ 10,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Resize 60%, scale up to min size",
                /* width= */ 50,
                /* height= */ 50,
                /* targetWidth= */ 48,
                /* targetHeight= */ 48,
                /* startWidth= */ 50,
                /* startHeight= */ 50,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Resize 60%, scale up to min size",
                /* width= */ 80,
                /* height= */ 80,
                /* targetWidth= */ 48,
                /* targetHeight= */ 48,
                /* startWidth= */ 80,
                /* startHeight= */ 80,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Resize 60%, scale up to min size (shorter height)",
                /* width= */ 50,
                /* height= */ 25,
                /* targetWidth= */ 96,
                /* targetHeight= */ 48,
                /* startWidth= */ 50,
                /* startHeight= */ 25,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Resize 60%, scale up to min size (shorter width)",
                /* width= */ 25,
                /* height= */ 50,
                /* targetWidth= */ 48,
                /* targetHeight= */ 96,
                /* startWidth= */ 25,
                /* startHeight= */ 50,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
    }

    @Test
    public void testResizeShadowImage_ScaleDownToMax() {
        doTestResizeShadowImage(
                "Scale down to max width",
                /* width= */ 584,
                /* height= */ 584,
                /* targetWidth= */ 210,
                /* targetHeight= */ 210,
                /* startWidth= */ 584,
                /* startHeight= */ 584,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Scale 60% and adjust to max width",
                /* width= */ 600,
                /* height= */ 600,
                /* targetWidth= */ 210,
                /* targetHeight= */ 210,
                /* startWidth= */ 600,
                /* startHeight= */ 600,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 0);

        doTestResizeShadowImage(
                "Scale 60%, adjust to max width, adjust short side (height) to min size, "
                        + "adjust long side to max width",
                /* width= */ 800,
                /* height= */ 80,
                /* targetWidth= */ 350,
                /* targetHeight= */ 48,
                /* startWidth= */ 583,
                /* startHeight= */ 80,
                /* truncatedWidth= */ 108,
                /* truncatedHeight= */ 0);
        doTestResizeShadowImage(
                "Scale 60%, adjust to max width, adjust short side (width) to min size, "
                        + "adjust long side to max height",
                /* width= */ 80,
                /* height= */ 800,
                /* targetWidth= */ 48,
                /* targetHeight= */ 210,
                /* startWidth= */ 80,
                /* startHeight= */ 350,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 225);

        doTestResizeShadowImage(
                "Scale 60%, adjust long side to max height",
                /* width= */ 150,
                /* height= */ 1500,
                /* targetWidth= */ 48,
                /* targetHeight= */ 210,
                /* startWidth= */ 150,
                /* startHeight= */ 656,
                /* truncatedWidth= */ 0,
                /* truncatedHeight= */ 422);
    }

    private void doTestResizeShadowImage(
            String testcase,
            int width,
            int height,
            int targetWidth,
            int targetHeight,
            int startWidth,
            int startHeight,
            int truncatedWidth,
            int truncatedHeight) {
        DragShadowSpec dragShadowSpec =
                AnimatedImageDragShadowBuilder.getDragShadowSpec(
                        mContext, width, height, WINDOW_WIDTH, WINDOW_HEIGHT);

        final int actualTargetWidth = dragShadowSpec.targetWidth;
        final int actualTargetHeight = dragShadowSpec.targetHeight;
        String assertMsg =
                "Test case <"
                        + testcase
                        + "> Input Size <"
                        + width
                        + " * "
                        + height
                        + "> Expected target size <"
                        + targetWidth
                        + "*"
                        + targetHeight
                        + "> Actual target size <"
                        + actualTargetWidth
                        + "*"
                        + actualTargetHeight
                        + ">";
        Assert.assertTrue(
                assertMsg, targetWidth == actualTargetWidth && targetHeight == actualTargetHeight);

        final int actualStartWidth = dragShadowSpec.startWidth;
        final int actualStartHeight = dragShadowSpec.startHeight;
        assertMsg =
                "Test case <"
                        + testcase
                        + "> Input Size <"
                        + width
                        + " * "
                        + height
                        + "> Expected start size <"
                        + startWidth
                        + "*"
                        + startHeight
                        + "> Actual start size <"
                        + actualStartWidth
                        + "*"
                        + actualStartHeight
                        + ">";
        Assert.assertTrue(
                assertMsg, startWidth == actualStartWidth && startHeight == actualStartHeight);

        final int actualTruncatedWidth = dragShadowSpec.truncatedWidth;
        final int actualTruncatedHeight = dragShadowSpec.truncatedHeight;
        assertMsg =
                "Test case <"
                        + testcase
                        + "> Input Size <"
                        + width
                        + " * "
                        + height
                        + "> Expected truncated size <"
                        + truncatedWidth
                        + "*"
                        + truncatedHeight
                        + "> Actual truncated size <"
                        + actualTruncatedWidth
                        + "*"
                        + actualTruncatedHeight
                        + ">";
        Assert.assertTrue(
                assertMsg,
                truncatedWidth == actualTruncatedWidth && truncatedHeight == actualTruncatedHeight);
    }

    @Test
    public void testAdjustCursorOffset_WithoutTruncate() {
        // Width == Height;
        DragShadowSpec dragShadowSpec = new DragShadowSpec(100, 100, 60, 60, 0, 0);
        doTestAdjustCursorOffset(
                "Image is not truncated",
                /* cursorOffsetX= */ 50,
                /* cursorOffsetY= */ 50,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 100,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 50,
                /* expectedCursorOffsetY= */ 50);
        doTestAdjustCursorOffset(
                "Image is not truncated",
                /* cursorOffsetX= */ 40,
                /* cursorOffsetY= */ 40,
                /* dragObjRectWidth= */ 200,
                /* dragObjRectHeight= */ 200,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 20,
                /* expectedCursorOffsetY= */ 20);

        // Width != Height;
        dragShadowSpec = new DragShadowSpec(200, 100, 120, 60, 0, 0);
        doTestAdjustCursorOffset(
                "Image is not truncated, Width != Height",
                /* cursorOffsetX= */ 100,
                /* cursorOffsetY= */ 50,
                /* dragObjRectWidth= */ 200,
                /* dragObjRectHeight= */ 100,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 100,
                /* expectedCursorOffsetY= */ 50);
        doTestAdjustCursorOffset(
                "Image is not truncated, Width != Height",
                /* cursorOffsetX= */ 80,
                /* cursorOffsetY= */ 40,
                /* dragObjRectWidth= */ 400,
                /* dragObjRectHeight= */ 200,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 40,
                /* expectedCursorOffsetY= */ 20);
    }

    @Test
    public void testAdjustCursorOffset_TruncateOnHeight() {
        DragShadowSpec dragShadowSpec = new DragShadowSpec(5, 40, 50, 400, 0, 80);
        doTestAdjustCursorOffset(
                "Image is truncated along height, touch point in the middle",
                /* cursorOffsetX= */ 2,
                /* cursorOffsetY= */ 100,
                /* dragObjRectWidth= */ 5,
                /* dragObjRectHeight= */ 200,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 2,
                /* expectedCursorOffsetY= */ 20);
        doTestAdjustCursorOffset(
                "Image is truncated along height, touch point at the top",
                /* cursorOffsetX= */ 2,
                /* cursorOffsetY= */ 10,
                /* dragObjRectWidth= */ 5,
                /* dragObjRectHeight= */ 200,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 2,
                /* expectedCursorOffsetY= */ 0);
        doTestAdjustCursorOffset(
                "Image is truncated along height, touch point at the bottom",
                /* cursorOffsetX= */ 2,
                /* cursorOffsetY= */ 190,
                /* dragObjRectWidth= */ 5,
                /* dragObjRectHeight= */ 200,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 2,
                /* expectedCursorOffsetY= */ 40);
    }

    @Test
    public void testAdjustCursorOffset_TruncateOnWidth() {
        DragShadowSpec dragShadowSpec = new DragShadowSpec(40, 5, 400, 50, 80, 0);
        doTestAdjustCursorOffset(
                "Image is truncated along width, touch point in the middle",
                /* cursorOffsetX= */ 100,
                /* cursorOffsetY= */ 2,
                /* dragObjRectWidth= */ 200,
                /* dragObjRectHeight= */ 5,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 20,
                /* expectedCursorOffsetY= */ 2);
        doTestAdjustCursorOffset(
                "Image is truncated along width, touch point on the left",
                /* cursorOffsetX= */ 10,
                /* cursorOffsetY= */ 2,
                /* dragObjRectWidth= */ 200,
                /* dragObjRectHeight= */ 5,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 0,
                /* expectedCursorOffsetY= */ 2);
        doTestAdjustCursorOffset(
                "Image is truncated along width, touch point on the right",
                /* cursorOffsetX= */ 190,
                /* cursorOffsetY= */ 2,
                /* dragObjRectWidth= */ 200,
                /* dragObjRectHeight= */ 5,
                /* dragShadowSpec= */ dragShadowSpec,
                /* expectedCursorOffsetX= */ 40,
                /* expectedCursorOffsetY= */ 2);
    }

    private void doTestAdjustCursorOffset(
            String testcase,
            int cursorOffsetX,
            int cursorOffsetY,
            int dragObjRectWidth,
            int dragObjRectHeight,
            DragShadowSpec dragShadowSpec,
            int expectedCursorOffsetX,
            int expectedCursorOffsetY) {
        CursorOffset cursorOffset =
                AnimatedImageDragShadowBuilder.adjustCursorOffset(
                        cursorOffsetX,
                        cursorOffsetY,
                        dragObjRectWidth,
                        dragObjRectHeight,
                        dragShadowSpec);

        final int actualCursorOffsetX = (int) cursorOffset.x;
        final int actualCursorOffsetY = (int) cursorOffset.y;
        String assertMsg =
                "Test case <"
                        + testcase
                        + "> Expected cursor offset <"
                        + expectedCursorOffsetX
                        + "*"
                        + expectedCursorOffsetY
                        + "> Actual cursor offset <"
                        + actualCursorOffsetX
                        + "*"
                        + actualCursorOffsetY
                        + ">";
        Assert.assertTrue(
                assertMsg,
                expectedCursorOffsetX == actualCursorOffsetX
                        && expectedCursorOffsetY == actualCursorOffsetY);
    }
}

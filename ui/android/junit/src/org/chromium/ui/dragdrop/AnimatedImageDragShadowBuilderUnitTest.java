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
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl.DragShadowSpec;

/**
 * Unit tests for {@link AnimatedImageDragShadowBuilder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AnimatedImageDragShadowBuilderUnitTest {
    private static final int WINDOW_WIDTH = 1000;
    private static final int WINDOW_HEIGHT = 600;

    private final Context mContext = ApplicationProvider.getApplicationContext();

    @Test
    public void testResizeShadowImage_ScaleDownWithRatio() {
        doTestResizeShadowImage("Resize 60%",
                /*width=*/100, /*height=*/100,
                /*targetWidth=*/60, /*targetHeight=*/60,
                /*startWidth=*/100, /*startHeight=*/100, /*isTruncated=*/false);
        doTestResizeShadowImage("Resize 60%",
                /*width=*/82, /*height=*/82,
                /*targetWidth=*/49, /*targetHeight=*/49,
                /*startWidth=*/82, /*startHeight=*/82, /*isTruncated=*/false);
        doTestResizeShadowImage("Resize 60%",
                /*width=*/400, /*height=*/400,
                /*targetWidth=*/210, /*targetHeight=*/210,
                /*startWidth=*/400, /*startHeight=*/400, /*isTruncated=*/false);
    }

    @Test
    public void testResizeShadowImage_ScaleUpToRatio() {
        doTestResizeShadowImage("Scale up to min size",
                /*width=*/10, /*height=*/10,
                /*targetWidth=*/48, /*targetHeight=*/48,
                /*startWidth=*/10, /*startHeight=*/10, /*isTruncated=*/false);
        doTestResizeShadowImage("Resize 60%, scale up to min size",
                /*width=*/50, /*height=*/50,
                /*targetWidth=*/48, /*targetHeight=*/48,
                /*startWidth=*/50, /*startHeight=*/50, /*isTruncated=*/false);
        doTestResizeShadowImage("Resize 60%, scale up to min size",
                /*width=*/80, /*height=*/80,
                /*targetWidth=*/48, /*targetHeight=*/48,
                /*startWidth=*/80, /*startHeight=*/80, /*isTruncated=*/false);
        doTestResizeShadowImage("Resize 60%, scale up to min size (shorter height)",
                /*width=*/50, /*height=*/25,
                /*targetWidth=*/96, /*targetHeight=*/48,
                /*startWidth=*/50, /*startHeight=*/25, /*isTruncated=*/false);
        doTestResizeShadowImage("Resize 60%, scale up to min size (shorter width)",
                /*width=*/25, /*height=*/50,
                /*targetWidth=*/48, /*targetHeight=*/96,
                /*startWidth=*/25, /*startHeight=*/50, /*isTruncated=*/false);
    }

    @Test
    public void testResizeShadowImage_ScaleDownToMax() {
        doTestResizeShadowImage("Scale down to max width",
                /*width=*/584, /*height=*/584,
                /*targetWidth=*/210, /*targetHeight=*/210,
                /*startWidth=*/584, /*startHeight=*/584, /*isTruncated=*/false);
        doTestResizeShadowImage("Scale 60% and adjust to max width",
                /*width=*/600, /*height=*/600,
                /*targetWidth=*/210, /*targetHeight=*/210,
                /*startWidth=*/600, /*startHeight=*/600, /*isTruncated=*/false);

        doTestResizeShadowImage(
                "Scale 60%, adjust to max width, adjust short side (height) to min size, "
                        + "adjust long side to max width",
                /*width=*/800, /*height=*/80,
                /*targetWidth=*/350, /*targetHeight=*/48,
                /*startWidth=*/583, /*startHeight=*/80, /*isTruncated=*/true);
        doTestResizeShadowImage(
                "Scale 60%, adjust to max width, adjust short side (width) to min size, "
                        + "adjust long side to max height",
                /*width=*/80, /*height=*/800,
                /*targetWidth=*/48, /*targetHeight=*/210,
                /*startWidth=*/80, /*startHeight=*/350, /*isTruncated=*/true);

        doTestResizeShadowImage("Scale 60%, adjust long side to max height",
                /*width=*/150, /*height=*/1500,
                /*targetWidth=*/48, /*targetHeight=*/210,
                /*startWidth=*/150, /*startHeight=*/656, /*isTruncated=*/true);
    }

    private void doTestResizeShadowImage(String testcase, int width, int height, int targetWidth,
            int targetHeight, int startWidth, int startHeight, boolean isTruncated) {
        DragShadowSpec dragShadowSpec = AnimatedImageDragShadowBuilder.getDragShadowSpec(
                mContext, width, height, WINDOW_WIDTH, WINDOW_HEIGHT);

        final int actualTargetWidth = dragShadowSpec.targetWidth;
        final int actualTargetHeight = dragShadowSpec.targetHeight;
        String assertMsg = "Test case <" + testcase + "> Input Size <" + width + " * " + height
                + "> Expected target size <" + targetWidth + "*" + targetHeight
                + "> Actual target size <" + actualTargetWidth + "*" + actualTargetHeight + ">";
        Assert.assertTrue(
                assertMsg, targetWidth == actualTargetWidth && targetHeight == actualTargetHeight);

        final int actualStartWidth = dragShadowSpec.startWidth;
        final int actualStartHeight = dragShadowSpec.startHeight;
        assertMsg = "Test case <" + testcase + "> Input Size <" + width + " * " + height
                + "> Expected start size <" + startWidth + "*" + startHeight
                + "> Actual start size <" + actualStartWidth + "*" + actualStartHeight + ">";
        Assert.assertTrue(
                assertMsg, startWidth == actualStartWidth && startHeight == actualStartHeight);

        final boolean actualIsTruncated = dragShadowSpec.isTruncated;
        assertMsg = "Test case <" + testcase + "> Input Size <" + width + " * " + height
                + "> isTruncated should be " + isTruncated;
        Assert.assertEquals(assertMsg, isTruncated, actualIsTruncated);
    }
}

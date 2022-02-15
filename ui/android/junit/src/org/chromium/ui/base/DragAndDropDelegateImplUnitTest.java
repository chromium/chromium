// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.Pair;
import android.view.DragEvent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplay;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit test for DragAndDropDelegateImpl. Setting the device size to 1000*2000, scaleDensity = 1 for
 * the ease of dp / pixel calculation.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowDisplay.class}, qualifiers = "w1000dp-h2000dp-mdpi")
public class DragAndDropDelegateImplUnitTest {
    private Context mContext;
    private DragAndDropDelegateImpl mDragAndDropDelegateImpl;

    @Before
    public void setup() {
        mContext = ApplicationProvider.getApplicationContext();
        mDragAndDropDelegateImpl = new DragAndDropDelegateImpl();
    }

    @After
    public void tearDown() {
        DropDataContentProvider.clearCache();
    }

    @Test
    public void testStartDragAndDrop() {
        final View containerView = new View(mContext);
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid dropData = DropDataAndroid.create("text", null, null, null);

        mDragAndDropDelegateImpl.startDragAndDrop(containerView, shadowImage, dropData);

        Assert.assertTrue("Drag should be started.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals("Drag shadow width not match. Should not resize for text.", 100,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height not match. Should not resize for text.", 200,
                mDragAndDropDelegateImpl.getDragShadowHeight());

        DragEvent dragEvent = Mockito.mock(DragEvent.class);
        doReturn(DragEvent.ACTION_DRAG_ENDED).when(dragEvent).getAction();
        mDragAndDropDelegateImpl.onDrag(containerView, dragEvent);

        Assert.assertFalse("Drag should end.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals("Drag shadow width should be reset.", 0,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height should be reset.", 0,
                mDragAndDropDelegateImpl.getDragShadowHeight());
    }

    @Test
    public void testResizeShadowForDifferentDropData() {
        final View containerView = new View(mContext);
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);

        final DropDataAndroid textDropData = DropDataAndroid.create("text", null, null, null);
        mDragAndDropDelegateImpl.startDragAndDrop(containerView, shadowImage, textDropData);
        Assert.assertEquals("Drag shadow width not match. Should not resize for text.", 100,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height not match. Should not resize for text.", 200,
                mDragAndDropDelegateImpl.getDragShadowHeight());

        final DropDataAndroid linkDropData = DropDataAndroid.create(
                "text", JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), null, null);
        mDragAndDropDelegateImpl.startDragAndDrop(containerView, shadowImage, linkDropData);
        Assert.assertEquals("Drag shadow width not match. Should not resize for link.", 100,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height not match. Should not resize for link.", 200,
                mDragAndDropDelegateImpl.getDragShadowHeight());

        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png");
        mDragAndDropDelegateImpl.startDragAndDrop(containerView, shadowImage, imageDropData);
        Assert.assertEquals("Drag shadow width not match. Should do resize for image.", 60,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height not match. Should do resize for image.", 120,
                mDragAndDropDelegateImpl.getDragShadowHeight());

        Assert.assertNotNull("Cached Image bytes should not be null.",
                DropDataContentProvider.getImageBytesForTesting());
    }

    @Test
    public void testResizeShadowImage_ScaleDownWithRatio() {
        doTestResizeShadowImage("Resize 60%",
                /*width=*/100, /*height=*/100,
                /*expectedWidth=*/60, /*expectedHeight=*/60);
        doTestResizeShadowImage("Resize 60%",
                /*width=*/82, /*height=*/82,
                /*expectedWidth=*/49, /*expectedHeight=*/49);
        doTestResizeShadowImage("Resize 60%",
                /*width=*/583, /*height=*/583,
                /*expectedWidth=*/350, /*expectedHeight=*/350);
        doTestResizeShadowImage("Resize 60% (no min Height)",
                /*width=*/100, /*height=*/10,
                /*expectedWidth=*/60, /*expectedHeight=*/6);
    }

    @Test
    public void testResizeShadowImage_ScaleUpToRatio() {
        doTestResizeShadowImage("Scale up to minWidth",
                /*width=*/1, /*height=*/1,
                /*expectedWidth=*/48, /*expectedHeight=*/48);
        doTestResizeShadowImage("Resize 60%, scale up to minWidth",
                /*width=*/50, /*height=*/50,
                /*expectedWidth=*/48, /*expectedHeight=*/48);
        doTestResizeShadowImage("Resize 60%, scale up to minWidth",
                /*width=*/80, /*height=*/80,
                /*expectedWidth=*/48, /*expectedHeight=*/48);
        doTestResizeShadowImage("Resize 60%, scale up to minWidth (no min Height)",
                /*width=*/50, /*height=*/25,
                /*expectedWidth=*/48, /*expectedHeight=*/24);
    }

    @Test
    public void testResizeShadowImage_ScaleDownToMax() {
        doTestResizeShadowImage("Scale down to max width",
                /*width=*/584, /*height=*/584,
                /*expectedWidth=*/350, /*expectedHeight=*/350);
        doTestResizeShadowImage("Scale 60% and adjust to max width",
                /*width=*/600, /*height=*/600,
                /*expectedWidth=*/350, /*expectedHeight=*/350);

        doTestResizeShadowImage("Scale 60% and adjust to max width",
                /*width=*/1000, /*height=*/100,
                /*expectedWidth=*/350, /*expectedHeight=*/35);
        doTestResizeShadowImage("Scale 60% and adjust to max width (no min height)",
                /*width=*/1000, /*height=*/100,
                /*expectedWidth=*/350, /*expectedHeight=*/35);

        doTestResizeShadowImage("Scale 60% and adjust to max height",
                /*width=*/150, /*height=*/1500,
                /*expectedWidth=*/70, /*expectedHeight=*/700);
        doTestResizeShadowImage("Scale 60%, adjust to min width, and adjust to max height",
                /*width=*/15, /*height=*/1500,
                /*expectedWidth=*/7, /*expectedHeight=*/700);
    }

    private void doTestResizeShadowImage(
            String testcase, int width, int height, int expectedWidth, int expectedHeight) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ALPHA_8);

        Pair<Integer, Integer> widthHeight =
                DragAndDropDelegateImpl.getWidthHeightForScaleDragShadow(mContext, bitmap);

        final int actualResizedWidth = widthHeight.first;
        final int actualResizedHeight = widthHeight.second;

        String assertMsg = "Test case <" + testcase + "> Input Size <" + width + " * " + height
                + "> Expected size <" + expectedWidth + "*" + expectedHeight + "> Actual size <"
                + actualResizedWidth + "*" + actualResizedHeight + ">";

        Assert.assertTrue(assertMsg,
                expectedWidth == actualResizedWidth && expectedHeight == actualResizedHeight);
    }
}

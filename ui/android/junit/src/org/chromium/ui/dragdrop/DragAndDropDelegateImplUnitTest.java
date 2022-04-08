// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.mockito.Mockito.doReturn;

import android.content.ClipData;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.VectorDrawable;
import android.os.Build.VERSION_CODES;
import android.util.Pair;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.View.MeasureSpec;
import android.view.accessibility.AccessibilityManager;
import android.widget.ImageView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowAccessibilityManager;

import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl.DragTargetType;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for {@link DragAndDropDelegateImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowRecordHistogram.class})
public class DragAndDropDelegateImplUnitTest {
    /** Using a window size of 1000*600 for the ease of dp / pixel calculation. */
    private static final int WINDOW_WIDTH = 1000;
    private static final int WINDOW_HEIGHT = 600;

    private static final String IMAGE_FILENAME = "image.png";

    /** Helper shadow class to make sure #startDragAndDrop is accepted by Android. */
    @Implements(ApiHelperForN.class)
    static class ShadowApiHelperForN {
        static DragShadowBuilder sLastDragShadowBuilder;

        @Implementation
        public static boolean startDragAndDrop(View view, ClipData data,
                DragShadowBuilder shadowBuilder, Object myLocalState, int flags) {
            sLastDragShadowBuilder = shadowBuilder;
            return true;
        }
    }

    private Context mContext;
    private DragAndDropDelegateImpl mDragAndDropDelegateImpl;
    private View mContainerView;

    @Before
    public void setup() {
        mContext = ApplicationProvider.getApplicationContext();
        mDragAndDropDelegateImpl = new DragAndDropDelegateImpl();

        mContainerView = new View(mContext);
        View rootView = mContainerView.getRootView();
        rootView.measure(MeasureSpec.makeMeasureSpec(WINDOW_WIDTH, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(WINDOW_HEIGHT, MeasureSpec.EXACTLY));
        rootView.layout(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    @After
    public void tearDown() {
        DropDataContentProvider.onDragEnd(false);
        ShadowRecordHistogram.reset();
        ShadowApiHelperForN.sLastDragShadowBuilder = null;
    }

    @Test
    public void testStartDragAndDrop_Text() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid dropData = DropDataAndroid.create("text", null, null, null, null);

        mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, dropData);

        Assert.assertTrue("Drag should be started.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals("Drag shadow width not match. Should not resize for text.", 100,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height not match. Should not resize for text.", 200,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeNotRecorded("Drag didn't end.");

        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DRAG_ENDED));

        Assert.assertFalse("Drag should end.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals("Drag shadow width should be reset.", 0,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height should be reset.", 0,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeRecorded(DragTargetType.TEXT);
        assertDragOutsideWebContentHistogramsRecorded(/*dropResult=*/false);
    }

    @Test
    public void testStartDragAndDrop_Image() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);

        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, imageDropData);
        // width = scaledShadowSize + padding * 2 = 100 * 0.6 + 1 * 2 = 62
        Assert.assertEquals(
                "Drag shadow width not match. Should do resize for image and add 1dp border.", 62,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        // height = scaledShadowSize + padding * 2 = 200 * 0.6 + 1 * 2 = 122
        Assert.assertEquals(
                "Drag shadow height not match. Should do resize for image and add 1dp border.", 122,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        Assert.assertNotNull("Cached Image bytes should not be null.",
                DropDataContentProvider.getImageBytesForTesting());
        assertDragTypeNotRecorded("Drag didn't end.");

        DragEvent dragEnd = mockDragEvent(DragEvent.ACTION_DRAG_ENDED);
        mDragAndDropDelegateImpl.onDrag(mContainerView, dragEnd);
        Assert.assertNull("Cached Image bytes should be cleaned.",
                DropDataContentProvider.getImageBytesForTesting());
        assertDragTypeRecorded(DragTargetType.IMAGE);
        assertDragOutsideWebContentHistogramsRecorded(/*dropResult=*/false);
    }

    @Test
    public void testStartDragAndDrop_TextLink() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid dropData = DropDataAndroid.create(
                "text", JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), null, null, null);

        mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, dropData);

        Assert.assertTrue("Drag should start.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals("Drag shadow width does not match. Should not resize for text link.",
                100, mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height does not match. Should not resize for text link.",
                200, mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeNotRecorded("Drag did not end.");

        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DRAG_ENDED));

        Assert.assertFalse("Drag should end.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals("Drag shadow width should be reset.", 0,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals("Drag shadow height should be reset.", 0,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeRecorded(DragTargetType.LINK);
        assertDragOutsideWebContentHistogramsRecorded(/*dropResult=*/false);
    }

    @Test
    @Config(shadows = {ShadowApiHelperForN.class, ShadowAccessibilityManager.class})
    public void testStartDragAndDrop_NotSupportedForA11y() {
        final Bitmap shadowImage = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
        final DropDataAndroid dropData = DropDataAndroid.create("text", null, null, null, null);

        Assert.assertTrue("Drag and drop should start.",
                mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, dropData));

        AccessibilityManager a11yManager =
                (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        ShadowAccessibilityManager shadowA11yManager = Shadow.extract(a11yManager);
        shadowA11yManager.setEnabled(true);
        shadowA11yManager.setTouchExplorationEnabled(true);

        Assert.assertFalse("Drag and drop should not start when isTouchExplorationEnabled=true.",
                mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, dropData));
    }

    @Test
    @Config(shadows = {ShadowApiHelperForN.class})
    public void testDragImage_ShadowPlaceholder() {
        final Bitmap shadowImage = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, imageDropData);

        Assert.assertNotNull(
                "sLastDragShadowBuilder is null.", ShadowApiHelperForN.sLastDragShadowBuilder);
        View shadowView = ShadowApiHelperForN.sLastDragShadowBuilder.getView();
        Assert.assertTrue(
                "DrawShadowBuilder should host an ImageView.", shadowView instanceof ImageView);
        Assert.assertTrue(
                "Drag shadow image should host a globe icon, which should be a vector drawable.",
                ((ImageView) shadowView).getDrawable() instanceof VectorDrawable);
    }

    @Test
    public void testDragImage_DragHandled() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, imageDropData);

        final DragEvent dragEndEvent = mockDragEvent(DragEvent.ACTION_DRAG_ENDED);
        doReturn(true).when(dragEndEvent).getResult();
        mDragAndDropDelegateImpl.onDrag(mContainerView, dragEndEvent);

        Assert.assertNotNull("Cached Image bytes should not be cleaned, drag is handled.",
                DropDataContentProvider.getImageBytesForTesting());
        assertDragTypeRecorded(DragTargetType.IMAGE);
        assertDragOutsideWebContentHistogramsRecorded(/*dropResult=*/true);
    }

    @Test
    public void testDragImage_ReceivedDropBeforeDragEnds() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, shadowImage, imageDropData);

        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DROP));
        final DragEvent dragEndEvent = mockDragEvent(DragEvent.ACTION_DRAG_ENDED);
        doReturn(true).when(dragEndEvent).getResult();
        mDragAndDropDelegateImpl.onDrag(mContainerView, dragEndEvent);

        // Drop on the same view does not lead to recording of drag duration.
        assertDragTypeNotRecorded("Drag dropped on the same view.");
        assertDropInWebContentHistogramsRecorded();
        Assert.assertNull("Cached Image bytes should be cleaned since drop is not handled.",
                DropDataContentProvider.getImageBytesForTesting());
    }

    @Test
    public void testIgnoreDragStartedElsewhere() {
        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DROP));
        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DRAG_ENDED));

        assertDragTypeNotRecorded("Drag dropped on the same view.");
        assertHistogramRecorded("Android.DragDrop.FromWebContent.DropInWebContent.Duration", false,
                "Only tracking drag started by mDragAndDropDelegateImpl#startDragAndDrop.");
        assertHistogramRecorded("Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip",
                false, "Only tracking drag started by mDragAndDropDelegateImpl#startDragAndDrop.");
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
                /*width=*/351, /*height=*/351,
                /*expectedWidth=*/210, /*expectedHeight=*/210);
    }

    @Test
    public void testResizeShadowImage_ScaleUpToRatio() {
        doTestResizeShadowImage("Scale up to min size",
                /*width=*/1, /*height=*/1,
                /*expectedWidth=*/48, /*expectedHeight=*/48);
        doTestResizeShadowImage("Resize 60%, scale up to min size",
                /*width=*/50, /*height=*/50,
                /*expectedWidth=*/48, /*expectedHeight=*/48);
        doTestResizeShadowImage("Resize 60%, scale up to min size",
                /*width=*/80, /*height=*/80,
                /*expectedWidth=*/48, /*expectedHeight=*/48);
        doTestResizeShadowImage("Resize 60%, scale up to min size (shorter height)",
                /*width=*/50, /*height=*/25,
                /*expectedWidth=*/96, /*expectedHeight=*/48);
        doTestResizeShadowImage("Resize 60%, scale up to min size (shorter width)",
                /*width=*/25, /*height=*/50,
                /*expectedWidth=*/48, /*expectedHeight=*/96);
    }

    @Test
    public void testResizeShadowImage_ScaleDownToMax() {
        doTestResizeShadowImage("Scale down to max width",
                /*width=*/584, /*height=*/584,
                /*expectedWidth=*/210, /*expectedHeight=*/210);
        doTestResizeShadowImage("Scale 60% and adjust to max width",
                /*width=*/600, /*height=*/600,
                /*expectedWidth=*/210, /*expectedHeight=*/210);

        doTestResizeShadowImage(
                "Scale 60%, adjust to max width, adjust short side (height) to min size, adjust long side to max width",
                /*width=*/800, /*height=*/80,
                /*expectedWidth=*/350, /*expectedHeight=*/48);
        doTestResizeShadowImage(
                "Scale 60%, adjust to max width, adjust short side (width) to min size, adjust long side to max height",
                /*width=*/80, /*height=*/800,
                /*expectedWidth=*/48, /*expectedHeight=*/210);

        doTestResizeShadowImage("Scale 60%, adjust long side to max height",
                /*width=*/150, /*height=*/1500,
                /*expectedWidth=*/48, /*expectedHeight=*/210);
    }

    @Test
    public void testTextForLinkData_UrlWithNoTitle() {
        final DropDataAndroid dropData = DropDataAndroid.create(
                "", JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), null, null, null);

        String text = DragAndDropDelegateImpl.getTextForLinkData(dropData);
        Assert.assertEquals("Text should match.", JUnitTestGURLs.EXAMPLE_URL, text);
    }

    @Test
    public void testTextForLinkData_UrlWithTitle() {
        String linkTitle = "Link text";
        final DropDataAndroid dropData = DropDataAndroid.create(
                linkTitle, JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), null, null, null);

        String text = DragAndDropDelegateImpl.getTextForLinkData(dropData);
        Assert.assertEquals(
                "Text should match.", linkTitle + "\n" + JUnitTestGURLs.EXAMPLE_URL, text);
    }

    @Test
    @Config(sdk = VERSION_CODES.O)
    public void testClipData_ImageWithUrl() {
        final DropDataAndroid dropData =
                DropDataAndroid.create("", JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL),
                        new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);

        ClipData clipData = mDragAndDropDelegateImpl.buildClipData(dropData);
        Assert.assertEquals(
                "Image ClipData should include image and URL info.", 2, clipData.getItemCount());
        Assert.assertEquals("Image URL info should match.", JUnitTestGURLs.EXAMPLE_URL,
                clipData.getItemAt(1).getText());
    }

    private void doTestResizeShadowImage(
            String testcase, int width, int height, int expectedWidth, int expectedHeight) {
        Pair<Integer, Integer> widthHeight =
                DragAndDropDelegateImpl.getWidthHeightForScaleDragShadow(
                        mContext, width, height, WINDOW_WIDTH, WINDOW_HEIGHT);

        final int actualResizedWidth = widthHeight.first;
        final int actualResizedHeight = widthHeight.second;

        String assertMsg = "Test case <" + testcase + "> Input Size <" + width + " * " + height
                + "> Expected size <" + expectedWidth + "*" + expectedHeight + "> Actual size <"
                + actualResizedWidth + "*" + actualResizedHeight + ">";

        Assert.assertTrue(assertMsg,
                expectedWidth == actualResizedWidth && expectedHeight == actualResizedHeight);
    }

    private DragEvent mockDragEvent(int action) {
        DragEvent event = Mockito.mock(DragEvent.class);
        doReturn(action).when(event).getAction();
        return event;
    }

    private void assertDragTypeNotRecorded(String reason) {
        assertHistogramRecorded("Android.DragDrop.FromWebContent.TargetType", false, reason);
    }

    private void assertDragTypeRecorded(@DragTargetType int type) {
        final String histogram = "Android.DragDrop.FromWebContent.TargetType";
        final String errorMsg = "<" + histogram + "> is not recorded correctly.";
        Assert.assertEquals(errorMsg, 1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(histogram, type));
    }

    private void assertDragOutsideWebContentHistogramsRecorded(boolean dropResult) {
        // Verify drop outside metrics recorded.
        final String histogram =
                "Android.DragDrop.FromWebContent.Duration." + (dropResult ? "Success" : "Canceled");
        assertHistogramRecorded(histogram, true, "Drop outside of web content.");

        // Verify drop inside metrics not recorded.
        assertHistogramRecorded("Android.DragDrop.FromWebContent.DropInWebContent.Duration", false,
                "Drop outside of web content.");
        assertHistogramRecorded("Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip",
                false, "Drop outside of web content.");
    }

    private void assertDropInWebContentHistogramsRecorded() {
        // Verify drop inside metrics recorded.
        assertHistogramRecorded("Android.DragDrop.FromWebContent.DropInWebContent.Duration", true,
                "Drop inside web content.");
        assertHistogramRecorded("Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip",
                true, "Drop inside web content.");

        // Verify drop outside metrics not recorded.
        assertHistogramRecorded("Android.DragDrop.FromWebContent.Duration.Success", false,
                "Should not recorded when drop inside web content.");
        assertHistogramRecorded("Android.DragDrop.FromWebContent.Duration.Canceled", false,
                "Should not recorded when drop inside web content.");
    }

    private void assertHistogramRecorded(String histogram, boolean recorded, String reason) {
        Assert.assertEquals(
                String.format("<%s> is not recorded correctly. Reason: %s", histogram, reason),
                recorded ? 1 : 0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(histogram));
    }
}

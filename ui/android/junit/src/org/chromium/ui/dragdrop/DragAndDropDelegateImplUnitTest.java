// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ProviderInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.VectorDrawable;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.View.MeasureSpec;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowContentResolver;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl.DragTargetType;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link DragAndDropDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({UiAndroidFeatureList.DRAG_DROP_EMPTY})
public class DragAndDropDelegateImplUnitTest {
    /** Using a window size of 1000*600 for the ease of dp / pixel calculation. */
    private static final int WINDOW_WIDTH = 1000;

    private static final int WINDOW_HEIGHT = 600;
    private static final float DRAG_START_X_DP = 1.0f;
    private static final float DRAG_START_Y_DP = 1.0f;
    private static final String IMAGE_FILENAME = "image.png";

    @Mock private DragAndDropPermissions mDragAndDropPermissions;
    @Mock private DragAndDropBrowserDelegate mDragAndDropBrowserDelegate;
    @Spy private View mContainerView;

    private DragAndDropDelegateImpl mDragAndDropDelegateImpl;
    private DropDataProviderImpl mDropDataProviderImpl;
    private @Nullable DragShadowBuilder mLastDragShadowBuilder;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);

        Context context = ContextUtils.getApplicationContext();
        mDropDataProviderImpl = new DropDataProviderImpl();
        mDragAndDropDelegateImpl = new DragAndDropDelegateImpl();
        DropDataContentProvider provider = new DropDataContentProvider();
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = DropDataProviderImpl.FULL_AUTH_URI.getAuthority();
        provider.attachInfo(context, providerInfo);
        provider.setDropDataProviderImpl(mDropDataProviderImpl);

        ShadowContentResolver.registerProviderInternal(
                DropDataProviderImpl.FULL_AUTH_URI.getAuthority(), provider);
        mContainerView = Mockito.spy(new View(context));
        doAnswer(
                        invocationOnMock -> {
                            mLastDragShadowBuilder = invocationOnMock.getArgument(1);
                            return true;
                        })
                .when(mContainerView)
                .startDragAndDrop(any(), any(DragShadowBuilder.class), any(), anyInt());
        View rootView = mContainerView.getRootView();
        rootView.measure(
                MeasureSpec.makeMeasureSpec(WINDOW_WIDTH, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(WINDOW_HEIGHT, MeasureSpec.EXACTLY));
        rootView.layout(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    @After
    public void tearDown() {
        mDropDataProviderImpl.onDragEnd(false);
        AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false);
    }

    @Test
    public void testStartDragAndDrop_Text() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid dropData = DropDataAndroid.create("text", null, null, null, null);

        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                dropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);

        Assert.assertTrue("Drag should be started.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals(
                "Drag shadow width not match. Should not resize for text.",
                100,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals(
                "Drag shadow height not match. Should not resize for text.",
                200,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeNotRecorded("Drag didn't end.");

        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DRAG_ENDED));

        Assert.assertFalse("Drag should end.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals(
                "Drag shadow width should be reset.",
                0,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals(
                "Drag shadow height should be reset.",
                0,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeRecorded(DragTargetType.TEXT);
        assertDragOutsideWebContentHistogramsRecorded(/* dropResult= */ false);
    }

    @Test
    @Config(shadows = ShadowContentResolver.class)
    public void testStartDragAndDrop_Image() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                imageDropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);
        Assert.assertFalse(
                "The DragShadowBuilder should not be AnimatedImageDragShadowBuilder.",
                mLastDragShadowBuilder instanceof AnimatedImageDragShadowBuilder);
        // width = scaledShadowSize + padding * 2 = 100 * 0.6 + 1 * 2 = 62
        Assert.assertEquals(
                "Drag shadow width not match. Should do resize for image and add 1dp border.",
                62,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        // height = scaledShadowSize + padding * 2 = 200 * 0.6 + 1 * 2 = 122
        Assert.assertEquals(
                "Drag shadow height not match. Should do resize for image and add 1dp border.",
                122,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        Assert.assertNotNull(
                "Cached Image bytes should not be null.",
                mDropDataProviderImpl.getImageBytesForTesting());
        assertDragTypeNotRecorded("Drag didn't end.");

        DragEvent dragEnd = mockDragEvent(DragEvent.ACTION_DRAG_ENDED);
        mDragAndDropDelegateImpl.onDrag(mContainerView, dragEnd);
        Assert.assertNull(
                "Cached Image bytes should be cleaned.",
                mDropDataProviderImpl.getImageBytesForTesting());
        assertDragTypeRecorded(DragTargetType.IMAGE);
        assertDragOutsideWebContentHistogramsRecorded(/* dropResult= */ false);
    }

    @Test
    @Config(shadows = ShadowContentResolver.class)
    public void testStartDragAndDrop_Image_WithAnimation() {
        mDragAndDropDelegateImpl.setDragAndDropBrowserDelegate(
                mockDragAndDropBrowserDelegate(false, true, null, null));
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                imageDropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);
        Assert.assertTrue(
                "The DragShadowBuilder should be AnimatedImageDragShadowBuilder.",
                mLastDragShadowBuilder instanceof AnimatedImageDragShadowBuilder);
        // width = scaledShadowSize + padding * 2 = 100 * 0.6 + 1 * 2 = 62
        Assert.assertEquals(
                "Drag shadow width not match. Should do resize for image and add 1dp border.",
                62,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        // height = scaledShadowSize + padding * 2 = 200 * 0.6 + 1 * 2 = 122
        Assert.assertEquals(
                "Drag shadow height not match. Should do resize for image and add 1dp border.",
                122,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        Assert.assertNotNull(
                "Cached Image bytes should not be null.",
                mDropDataProviderImpl.getImageBytesForTesting());
        assertDragTypeNotRecorded("Drag didn't end.");

        DragEvent dragEnd = mockDragEvent(DragEvent.ACTION_DRAG_ENDED);
        mDragAndDropDelegateImpl.onDrag(mContainerView, dragEnd);
        Assert.assertNull(
                "Cached Image bytes should be cleaned.",
                mDropDataProviderImpl.getImageBytesForTesting());
        assertDragTypeRecorded(DragTargetType.IMAGE);
        assertDragOutsideWebContentHistogramsRecorded(/* dropResult= */ false);
    }

    @Test
    public void testStartDragAndDrop_TextLink() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid dropData =
                DropDataAndroid.create("text", JUnitTestGURLs.EXAMPLE_URL, null, null, null);

        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                dropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);

        Assert.assertTrue("Drag should start.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals(
                "Drag shadow width does not match. Should not resize for text link.",
                100,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals(
                "Drag shadow height does not match. Should not resize for text link.",
                200,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeNotRecorded("Drag did not end.");

        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DRAG_ENDED));

        Assert.assertFalse("Drag should end.", mDragAndDropDelegateImpl.isDragStarted());
        Assert.assertEquals(
                "Drag shadow width should be reset.",
                0,
                mDragAndDropDelegateImpl.getDragShadowWidth());
        Assert.assertEquals(
                "Drag shadow height should be reset.",
                0,
                mDragAndDropDelegateImpl.getDragShadowHeight());
        assertDragTypeRecorded(DragTargetType.LINK);
        assertDragOutsideWebContentHistogramsRecorded(/* dropResult= */ false);
    }

    @Test
    public void testStartDragAndDrop_NotSupportedForA11y() {
        final Bitmap shadowImage = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
        final DropDataAndroid dropData = DropDataAndroid.create("text", null, null, null, null);

        Assert.assertTrue(
                "Drag and drop should start.",
                mDragAndDropDelegateImpl.startDragAndDrop(
                        mContainerView,
                        shadowImage,
                        dropData,
                        mContainerView.getContext(),
                        /* cursorOffsetX= */ 0,
                        /* cursorOffsetY= */ 0,
                        /* dragObjRectWidth= */ 100,
                        /* dragObjRectHeight= */ 200));

        AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(true);
        Assert.assertFalse(
                "Drag and drop should not start when isAnyAccessibilityServiceEnabled=true.",
                mDragAndDropDelegateImpl.startDragAndDrop(
                        mContainerView,
                        shadowImage,
                        dropData,
                        mContainerView.getContext(),
                        /* cursorOffsetX= */ 0,
                        /* cursorOffsetY= */ 0,
                        /* dragObjRectWidth= */ 100,
                        /* dragObjRectHeight= */ 200));
    }

    @Test
    @DisableFeatures({UiAndroidFeatureList.DRAG_DROP_EMPTY})
    public void testStartDragAndDrop_InvalidDropData() {
        final DropDataAndroid dropData = DropDataAndroid.create(null, null, null, null, null);

        Assert.assertFalse(
                "Drag and drop should not start.",
                mDragAndDropDelegateImpl.startDragAndDrop(
                        mContainerView,
                        Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888),
                        dropData,
                        mContainerView.getContext(),
                        /* cursorOffsetX= */ 0,
                        /* cursorOffsetY= */ 0,
                        /* dragObjRectWidth= */ 100,
                        /* dragObjRectHeight= */ 200));
    }

    @Test
    public void testStartDragAndDrop_EmptyDropData() {
        final DropDataAndroid dropData = DropDataAndroid.create(null, null, null, null, null);

        Assert.assertTrue(
                "Drag and drop should start.",
                mDragAndDropDelegateImpl.startDragAndDrop(
                        mContainerView,
                        Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888),
                        dropData,
                        mContainerView.getContext(),
                        /* cursorOffsetX= */ 0,
                        /* cursorOffsetY= */ 0,
                        /* dragObjRectWidth= */ 100,
                        /* dragObjRectHeight= */ 200));
        Assert.assertTrue("Drag should be started.", mDragAndDropDelegateImpl.isDragStarted());
    }

    @Test
    public void testStartDragAndDrop_WithDragShadowBuilder() {
        final DropDataAndroid dropData = DropDataAndroid.create("text", null, null, null, null);
        DragShadowBuilder mockBuilder = mock(DragShadowBuilder.class);
        Assert.assertTrue(
                "Drag and drop should start.",
                mDragAndDropDelegateImpl.startDragAndDrop(mContainerView, mockBuilder, dropData));
        Assert.assertTrue("Drag should be started.", mDragAndDropDelegateImpl.isDragStarted());
    }

    @Test
    public void testDragImage_ShadowPlaceholder() {
        final Bitmap shadowImage = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                imageDropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);

        Assert.assertNotNull("LastDragShadowBuilder is null.", mLastDragShadowBuilder);
        View shadowView = mLastDragShadowBuilder.getView();
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
        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                imageDropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);

        final DragEvent dragEndEvent = mockDragEvent(DragEvent.ACTION_DRAG_ENDED);
        doReturn(true).when(dragEndEvent).getResult();
        mDragAndDropDelegateImpl.onDrag(mContainerView, dragEndEvent);

        Assert.assertNotNull(
                "Cached Image bytes should not be cleaned, drag is handled.",
                mDropDataProviderImpl.getImageBytesForTesting());
        assertDragTypeRecorded(DragTargetType.IMAGE);
        assertDragOutsideWebContentHistogramsRecorded(/* dropResult= */ true);
    }

    @Test
    @EnableFeatures({UiAndroidFeatureList.DRAG_DROP_FILES})
    public void testDragImage_ReceivedDropBeforeDragEnds() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                imageDropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);

        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DROP));
        final DragEvent dragEndEvent = mockDragEvent(DragEvent.ACTION_DRAG_ENDED);
        doReturn(true).when(dragEndEvent).getResult();
        mDragAndDropDelegateImpl.onDrag(mContainerView, dragEndEvent);

        // Drop on the same view does not lead to recording of drag duration.
        assertDragTypeNotRecorded("Drag dropped on the same view.");
        assertDropInWebContentHistogramsRecorded();
        Assert.assertNotNull(
                "Cached Image bytes should not be cleaned, drag is handled.",
                mDropDataProviderImpl.getImageBytesForTesting());
    }

    @Test
    public void testIgnoreDragStartedElsewhere() {
        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DROP));
        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DRAG_ENDED));

        assertDragTypeNotRecorded("Drag dropped on the same view.");
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.DropInWebContent.Duration",
                false,
                "Only tracking drag started by mDragAndDropDelegateImpl#startDragAndDrop.");
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip",
                false,
                "Only tracking drag started by mDragAndDropDelegateImpl#startDragAndDrop.");
    }

    @Test
    public void testDragStartedFromContainerView() {
        final Bitmap shadowImage = Bitmap.createBitmap(100, 200, Bitmap.Config.ALPHA_8);
        final DropDataAndroid imageDropData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        mDragAndDropDelegateImpl.startDragAndDrop(
                mContainerView,
                shadowImage,
                imageDropData,
                mContainerView.getContext(),
                /* cursorOffsetX= */ 0,
                /* cursorOffsetY= */ 0,
                /* dragObjRectWidth= */ 100,
                /* dragObjRectHeight= */ 200);

        mDragAndDropDelegateImpl.onDrag(
                mContainerView, mockDragEvent(DragEvent.ACTION_DRAG_STARTED));
        Assert.assertEquals(
                "Recorded drag start X dp should match.",
                DRAG_START_X_DP,
                mDragAndDropDelegateImpl.getDragStartXDp(),
                0.0f);
        Assert.assertEquals(
                "Recorded drag start Y dp should match.",
                DRAG_START_Y_DP,
                mDragAndDropDelegateImpl.getDragStartYDp(),
                0.0f);
    }

    @Test
    public void testTextForLinkData_UrlWithNoTitle() {
        final DropDataAndroid dropData =
                DropDataAndroid.create("", JUnitTestGURLs.EXAMPLE_URL, null, null, null);

        String text = DragAndDropDelegateImpl.getTextForLinkData(dropData);
        Assert.assertEquals("Text should match.", JUnitTestGURLs.EXAMPLE_URL.getSpec(), text);
    }

    @Test
    public void testTextForLinkData_UrlWithTitle() {
        String linkTitle = "Link text";
        final DropDataAndroid dropData =
                DropDataAndroid.create(linkTitle, JUnitTestGURLs.EXAMPLE_URL, null, null, null);

        String text = DragAndDropDelegateImpl.getTextForLinkData(dropData);
        Assert.assertEquals(
                "Text should match.",
                linkTitle + "\n" + JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                text);
    }

    @Test
    public void testClipData_ImageWithUrl() {
        final DropDataAndroid dropData =
                DropDataAndroid.create(
                        "",
                        JUnitTestGURLs.EXAMPLE_URL,
                        new byte[] {1, 2, 3, 4},
                        "png",
                        IMAGE_FILENAME);

        ClipData clipData = mDragAndDropDelegateImpl.buildClipData(dropData);
        Assert.assertEquals(
                "Image ClipData should only include image.", 1, clipData.getItemCount());
        Assert.assertNotNull("Image Uri should exist.", clipData.getItemAt(0).getUri());
    }

    @Test
    public void testClipData_TextLink_NonNullIntent() {
        final DropDataAndroid dropData =
                DropDataAndroid.create("", JUnitTestGURLs.EXAMPLE_URL, null, null, null);
        mDragAndDropDelegateImpl.setDragAndDropBrowserDelegate(
                mockDragAndDropBrowserDelegate(false, false, null, new Intent()));
        ClipData clipData = mDragAndDropDelegateImpl.buildClipData(dropData);
        Assert.assertTrue(
                "Link ClipData should include plaintext MIME type.",
                clipData.getDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN));
        Assert.assertTrue(
                "Link ClipData should include intent MIME type.",
                clipData.getDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_INTENT));
        Assert.assertEquals(
                "Dragged link text should match.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                clipData.getItemAt(0).getText());
        Assert.assertNotNull(
                "ClipData intent should not be null.", clipData.getItemAt(0).getIntent());
    }

    @Test
    public void testClipData_TextLink_NullIntent() {
        final DropDataAndroid dropData =
                DropDataAndroid.create("", JUnitTestGURLs.EXAMPLE_URL, null, null, null);
        mDragAndDropDelegateImpl.setDragAndDropBrowserDelegate(
                mockDragAndDropBrowserDelegate(false, false, null, null));
        ClipData clipData = mDragAndDropDelegateImpl.buildClipData(dropData);
        Assert.assertTrue(
                "Link ClipData should include plaintext MIME type.",
                clipData.getDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN));
        Assert.assertFalse(
                "Link ClipData should not include intent MIME type.",
                clipData.getDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_INTENT));
        Assert.assertEquals(
                "Dragged link text should match.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                clipData.getItemAt(0).getText());
    }

    @Test
    public void testClipData_browserContent() {
        ClipData mockClipData = mock(ClipData.class);
        DropDataAndroid mockDropData = mock(DropDataAndroid.class);
        when(mockDropData.hasBrowserContent()).thenReturn(true);
        when(mDragAndDropBrowserDelegate.buildClipData(mockDropData)).thenReturn(mockClipData);
        mDragAndDropDelegateImpl.setDragAndDropBrowserDelegate(mDragAndDropBrowserDelegate);
        ClipData actualClipData = mDragAndDropDelegateImpl.buildClipData(mockDropData);
        Assert.assertEquals("ClipData is not as expected.", mockClipData, actualClipData);
        verify(mDragAndDropBrowserDelegate).buildClipData(mockDropData);
    }

    @Test
    public void testBuildFlag_Link() {
        final DropDataAndroid data =
                DropDataAndroid.create("", JUnitTestGURLs.EXAMPLE_URL, null, null, null);
        int flag = mDragAndDropDelegateImpl.buildFlags(data);
        Assert.assertEquals("Expect flag(s): DRAG_FLAG_GLOBAL.", flag, View.DRAG_FLAG_GLOBAL);
    }

    @Test
    public void testBuildFlag_Text() {
        final DropDataAndroid data = DropDataAndroid.create("text", null, null, null, null);
        int flag = mDragAndDropDelegateImpl.buildFlags(data);
        Assert.assertEquals("Expect flag(s): DRAG_FLAG_GLOBAL.", flag, View.DRAG_FLAG_GLOBAL);
    }

    @Test
    public void testBuildFlag_LinkText() {
        final DropDataAndroid data =
                DropDataAndroid.create("text", JUnitTestGURLs.EXAMPLE_URL, null, null, null);
        int flag = mDragAndDropDelegateImpl.buildFlags(data);
        Assert.assertEquals("Expect flag(s): DRAG_FLAG_GLOBAL.", flag, View.DRAG_FLAG_GLOBAL);
    }

    @Test
    public void testBuildFlag_Image() {
        mDragAndDropDelegateImpl.setDragAndDropBrowserDelegate(mDragAndDropBrowserDelegate);
        doReturn(true).when(mDragAndDropBrowserDelegate).getSupportAnimatedImageDragShadow();
        final DropDataAndroid imageData =
                DropDataAndroid.create("", null, new byte[] {1, 2, 3, 4}, "png", IMAGE_FILENAME);
        int flag = mDragAndDropDelegateImpl.buildFlags(imageData);
        Assert.assertEquals(
                "Expect flag(s): DRAG_FLAG_GLOBAL | DRAG_FLAG_GLOBAL_URI_READ | DRAG_FLAG_OPAQUE.",
                flag,
                View.DRAG_FLAG_GLOBAL | View.DRAG_FLAG_GLOBAL_URI_READ | View.DRAG_FLAG_OPAQUE);
    }

    @Test
    public void testBuildFlag_ImageLink() {
        final DropDataAndroid imageData =
                DropDataAndroid.create(
                        "",
                        JUnitTestGURLs.EXAMPLE_URL,
                        new byte[] {1, 2, 3, 4},
                        "png",
                        IMAGE_FILENAME);
        int flag = mDragAndDropDelegateImpl.buildFlags(imageData);
        Assert.assertEquals(
                "Expect flag(s): DRAG_FLAG_GLOBAL | DRAG_FLAG_GLOBAL_URI_READ.",
                flag,
                View.DRAG_FLAG_GLOBAL | View.DRAG_FLAG_GLOBAL_URI_READ);
    }

    @Test
    public void testBuildFlag_BrowserContent() {
        final DropDataAndroid browserData =
                new DropDataAndroid(null, null, null, null, null) {
                    @Override
                    public boolean hasBrowserContent() {
                        return true;
                    }
                };
        int flag = mDragAndDropDelegateImpl.buildFlags(browserData);
        Assert.assertEquals(
                "Expect flag(s): DRAG_FLAG_GLOBAL | DRAG_FLAG_OPAQUE.",
                flag,
                View.DRAG_FLAG_GLOBAL | View.DRAG_FLAG_OPAQUE);
    }

    @Test
    public void testBuildFlag_Invalid() {
        final DropDataAndroid browserData = new DropDataAndroid(null, null, null, null, null);
        Assert.assertEquals(
                "Invalid data will not have flag set.",
                0,
                mDragAndDropDelegateImpl.buildFlags(browserData));
    }

    @Test
    public void testDropInChromeFromOutside() {
        mDragAndDropDelegateImpl.setDragAndDropBrowserDelegate(
                mockDragAndDropBrowserDelegate(true, false, mDragAndDropPermissions, null));
        // Assume that data is dragged from outside Chrome.
        mDragAndDropDelegateImpl.onDrag(mContainerView, mockDragEvent(DragEvent.ACTION_DROP));
        verify(mDragAndDropPermissions).release();
    }

    private DragEvent mockDragEvent(int action) {
        DragEvent event = Mockito.mock(DragEvent.class);
        when(event.getX()).thenReturn(DRAG_START_X_DP);
        when(event.getY()).thenReturn(DRAG_START_Y_DP);
        doReturn(action).when(event).getAction();
        return event;
    }

    private void assertDragTypeNotRecorded(String reason) {
        assertHistogramRecorded("Android.DragDrop.FromWebContent.TargetType", false, reason);
    }

    private void assertDragTypeRecorded(@DragTargetType int type) {
        final String histogram = "Android.DragDrop.FromWebContent.TargetType";
        final String errorMsg = "<" + histogram + "> is not recorded correctly.";
        Assert.assertEquals(
                errorMsg, 1, RecordHistogram.getHistogramValueCountForTesting(histogram, type));
    }

    private void assertDragOutsideWebContentHistogramsRecorded(boolean dropResult) {
        // Verify drop outside metrics recorded.
        final String histogram =
                "Android.DragDrop.FromWebContent.Duration." + (dropResult ? "Success" : "Canceled");
        assertHistogramRecorded(histogram, true, "Drop outside of web content.");

        // Verify drop inside metrics not recorded.
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.DropInWebContent.Duration",
                false,
                "Drop outside of web content.");
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip",
                false,
                "Drop outside of web content.");
    }

    private void assertDropInWebContentHistogramsRecorded() {
        // Verify drop inside metrics recorded.
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.DropInWebContent.Duration",
                true,
                "Drop inside web content.");
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip",
                true,
                "Drop inside web content.");

        // Verify drop outside metrics not recorded.
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.Duration.Success",
                false,
                "Should not recorded when drop inside web content.");
        assertHistogramRecorded(
                "Android.DragDrop.FromWebContent.Duration.Canceled",
                false,
                "Should not recorded when drop inside web content.");
    }

    private void assertHistogramRecorded(String histogram, boolean recorded, String reason) {
        Assert.assertEquals(
                String.format("<%s> is not recorded correctly. Reason: %s", histogram, reason),
                recorded ? 1 : 0,
                RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private DragAndDropBrowserDelegate mockDragAndDropBrowserDelegate(
            boolean supportDropInChrome,
            boolean supportAnimatedImageDragShadow,
            DragAndDropPermissions permissions,
            Intent intent) {
        when(mDragAndDropBrowserDelegate.getSupportDropInChrome()).thenReturn(supportDropInChrome);
        when(mDragAndDropBrowserDelegate.getSupportAnimatedImageDragShadow())
                .thenReturn(supportAnimatedImageDragShadow);
        when(mDragAndDropBrowserDelegate.getDragAndDropPermissions(any(DragEvent.class)))
                .thenReturn(permissions);
        when(mDragAndDropBrowserDelegate.createUrlIntent(anyString(), anyInt())).thenReturn(intent);
        return mDragAndDropBrowserDelegate;
    }
}

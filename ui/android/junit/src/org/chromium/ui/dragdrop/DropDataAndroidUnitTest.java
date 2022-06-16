// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.Context;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit test for {@link DropDataAndroid}. */
@RunWith(RobolectricTestRunner.class)
public class DropDataAndroidUnitTest {
    private static final String IMAGE_FILENAME = "image.webp";

    @Test
    public void testPlainText() {
        final String text = "text";
        final DropDataAndroid data = DropDataAndroid.create(text, null, null, null, null);

        assertDragData(data, /*isPlainText=*/true, /*hasLink=*/false, /*hasImage=*/false);
        Assert.assertEquals("Text does not match.", text, data.text);
    }

    @Test
    public void testLink() {
        final String text = "text";
        final GURL gurl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        final DropDataAndroid data = DropDataAndroid.create(text, gurl, null, null, null);

        assertDragData(data, /*isPlainText=*/false, /*hasLink=*/true, /*hasImage=*/false);
        Assert.assertEquals("Link does not match.", gurl, data.gurl);
    }

    @Test
    public void testImage() {
        final byte[] img = new byte[] {1, 2};
        final String imageExtension = "webp";
        final DropDataAndroid data =
                DropDataAndroid.create("", null, img, imageExtension, IMAGE_FILENAME);

        assertDragData(data, /*isPlainText=*/false, /*hasLink=*/false, /*hasImage=*/true);
        Assert.assertEquals("Image content does not match.", img, data.imageContent);
        Assert.assertEquals(
                "Image extension does not match.", imageExtension, data.imageContentExtension);
        Assert.assertEquals("Image filename does not match.", IMAGE_FILENAME, data.imageFilename);
    }

    @Test
    public void testImageLink() {
        final GURL gurl = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        final byte[] img = new byte[] {1, 2};
        final String imageExtension = "webp";
        final DropDataAndroid data =
                DropDataAndroid.create("", gurl, img, imageExtension, IMAGE_FILENAME);

        assertDragData(data, /*isPlainText=*/false, /*hasLink=*/true, /*hasImage=*/true);
        Assert.assertEquals("Link does not match.", gurl, data.gurl);
        Assert.assertEquals("Image content does not match.", img, data.imageContent);
        Assert.assertEquals(
                "Image extension does not match.", imageExtension, data.imageContentExtension);
        Assert.assertEquals("Image filename does not match.", IMAGE_FILENAME, data.imageFilename);
    }

    @Test
    public void testClipSupported() {
        ClipDescription descriptionText = new ClipDescription("text", new String[] {"text/plain"});
        ClipDescription descriptionImage =
                new ClipDescription("image", new String[] {"image/webp"});
        ClipDescription descriptionVideo = new ClipDescription("video", new String[] {"video/mp4"});
        ClipDescription descriptionApplication =
                new ClipDescription("intent", new String[] {"application/octet-stream"});

        Assert.assertTrue("Text dragging is supported.",
                DropDataAndroid.isClipContentSupported(descriptionText));
        Assert.assertTrue("Image dragging is supported.",
                DropDataAndroid.isClipContentSupported(descriptionImage));

        Assert.assertFalse("Video dragging is not supported.",
                DropDataAndroid.isClipContentSupported(descriptionVideo));
        Assert.assertFalse("Application dragging is not supported.",
                DropDataAndroid.isClipContentSupported(descriptionApplication));
    }

    // TODO(https://crbug.com/1261249): Add more test case for link / image once dropping these are
    // supported.
    @Test
    public void testCreateFromClipData_Text() {
        String text = "text";

        Context appContext = RuntimeEnvironment.getApplication().getApplicationContext();
        ClipData textClip = ClipData.newPlainText("label", text);
        DropDataAndroid dropData = DropDataAndroid.createFromClipData(textClip, appContext);

        assertDragData(dropData, /*isPlainText=*/true, /*hasLink=*/false, /*hasImage=*/false);
        Assert.assertEquals("DropData Text is different.", text, dropData.text);
    }

    private void assertDragData(
            DropDataAndroid data, boolean isPlainText, boolean hasLink, boolean hasImage) {
        Assert.assertEquals("#isPlainText check failed.", isPlainText, data.isPlainText());
        Assert.assertEquals("#hasLink check failed.", hasLink, data.hasLink());
        Assert.assertEquals("#hasImage check failed.", hasImage, data.hasImage());
    }
}

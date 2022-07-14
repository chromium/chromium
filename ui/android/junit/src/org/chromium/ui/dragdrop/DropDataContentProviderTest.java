// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import static org.robolectric.Shadows.shadowOf;

import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.FileNotFoundException;
import java.util.concurrent.TimeUnit;

/**
 * Test basic functionality of {@link DropDataContentProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class DropDataContentProviderTest {
    private static final byte[] IMAGE_DATA_A = new byte[100];
    private static final byte[] IMAGE_DATA_B = new byte[50];
    private static final byte[] IMAGE_DATA_C = new byte[75];
    private static final String EXTENSION_A = "jpg";
    private static final String EXTENSION_B = "gif";
    private static final String EXTENSION_C = "png";
    private static final String IMAGE_FILENAME_A = "image.jpg";
    private static final String IMAGE_FILENAME_B = "image.gif";
    private static final String IMAGE_FILENAME_C = "image.png";
    private static final int CLEAR_CACHED_DATA_INTERVAL_MS = 10_000;

    private DropDataContentProvider mDropDataContentProvider;

    @Before
    public void setUp() {
        mDropDataContentProvider = new DropDataContentProvider();
        mDropDataContentProvider.onCreate();
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("jpg", "image/jpeg");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("gif", "image/gif");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("png", "image/png");
    }

    @After
    public void tearDown() {
        DropDataContentProvider.clearCache();
        DropDataContentProvider.clearLastUriCreatedTimestampForTesting();
        UmaRecorderHolder.resetForTesting();
    }

    @Test
    @SmallTest
    public void testCache() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        Assert.assertEquals("The MIME type for jpg file should be image/jpeg", "image/jpeg",
                mDropDataContentProvider.getType(uri));
        assertImageSizeRecorded(/*expectedCnt*/ 1);
        // Android.DragDrop.Image.UriCreatedInterval is not recorded for the first created Uri.
        assertImageUriCreatedIntervalRecorded(/*expectedCnt*/ 0);

        uri = DropDataContentProvider.cache(IMAGE_DATA_B, EXTENSION_B, IMAGE_FILENAME_B);
        Assert.assertEquals("The MIME type for gif file should be image/gif", "image/gif",
                mDropDataContentProvider.getType(uri));
        assertImageSizeRecorded(/*expectedCnt*/ 2);
        assertImageUriCreatedIntervalRecorded(/*expectedCnt*/ 1);

        uri = DropDataContentProvider.cache(IMAGE_DATA_C, EXTENSION_C, IMAGE_FILENAME_C);
        Assert.assertEquals("The MIME type for png file should be image/png", "image/png",
                mDropDataContentProvider.getType(uri));
        assertImageSizeRecorded(/*expectedCnt*/ 3);
        assertImageUriCreatedIntervalRecorded(/*expectedCnt*/ 2);
    }

    @Test
    @SmallTest
    public void testGetStreamTypes() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        String[] res = mDropDataContentProvider.getStreamTypes(uri, "image/*");
        Assert.assertEquals("res length should be 1 when uri matches the filter", 1, res.length);
        Assert.assertEquals(
                "The MIME type for jpg file should be image/jpeg", "image/jpeg", res[0]);

        res = mDropDataContentProvider.getStreamTypes(uri, "*/gif");
        Assert.assertNull("res should be null when uri does not match the filter", res);
    }

    @Test
    @SmallTest
    public void testQuery() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        Cursor cursor = mDropDataContentProvider.query(uri, null, null, null, null);
        Assert.assertEquals("The number of rows in the cursor should be 1", 1, cursor.getCount());
        Assert.assertEquals("The number of columns should be 2", 2, cursor.getColumnCount());
        cursor.moveToNext();
        int sizeIdx = cursor.getColumnIndex(OpenableColumns.SIZE);
        Assert.assertEquals("The file size should be 100", 100, cursor.getInt(sizeIdx));
        int displayNameIdx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
        Assert.assertEquals(
                "The file name should match.", IMAGE_FILENAME_A, cursor.getString(displayNameIdx));
        cursor.close();
    }

    @Test
    @SmallTest
    public void testClearCache() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        DropDataContentProvider.onDragEnd(false);
        Assert.assertNull("Image bytes should be null after clearing cache.",
                DropDataContentProvider.getImageBytesForTesting());
        Assert.assertNull("Handler should be null after clearing cache.",
                DropDataContentProvider.getHandlerForTesting());
        Assert.assertNull("MIME type should be null after clearing cache.",
                mDropDataContentProvider.getType(uri));
    }

    @Test
    @SmallTest
    public void testClearCacheWithDelay() throws FileNotFoundException {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        DropDataContentProvider.setClearCachedDataIntervalMs(CLEAR_CACHED_DATA_INTERVAL_MS);
        ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
        // #openFile could be called before or after the Android Drag End event.
        mDropDataContentProvider.openFile(uri, "r");
        DropDataContentProvider.onDragEnd(true);
        Assert.assertNotNull(
                "Image bytes should not be null immediately after clear cache with delay.",
                DropDataContentProvider.getImageBytesForTesting());
        Assert.assertNotNull("Handler should not be null after clear cache with delay.",
                DropDataContentProvider.getHandlerForTesting());
        Assert.assertEquals("The MIME type for jpg file should be image/jpeg", "image/jpeg",
                mDropDataContentProvider.getType(uri));
        assertImageFirstOpenFileRecorded(/*expectedCnt*/ 1);
        assertImageLastOpenFileRecorded(/*expectedCnt*/ 0);

        ShadowLooper.idleMainLooper(CLEAR_CACHED_DATA_INTERVAL_MS, TimeUnit.MILLISECONDS);
        Assert.assertNull("Image bytes should be null after the delayed time.",
                DropDataContentProvider.getImageBytesForTesting());
        Assert.assertNull("MIME type should be null after the delayed time.",
                mDropDataContentProvider.getType(uri));
        assertImageFirstOpenFileRecorded(/*expectedCnt*/ 1);
        assertImageLastOpenFileRecorded(/*expectedCnt*/ 1);
    }

    @Test
    @SmallTest
    public void testClearCacheWithDelayCancelled() throws FileNotFoundException {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        DropDataContentProvider.setClearCachedDataIntervalMs(CLEAR_CACHED_DATA_INTERVAL_MS);
        DropDataContentProvider.onDragEnd(true);
        ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
        mDropDataContentProvider.openFile(uri, "r");
        // Android.DragDrop.Image.UriCreatedInterval is not recorded for the first created Uri.
        assertImageUriCreatedIntervalRecorded(/*expectedCnt*/ 0);

        // Next image drag starts before the previous image expires.
        DropDataContentProvider.cache(IMAGE_DATA_B, EXTENSION_B, IMAGE_FILENAME_B);
        assertImageUriCreatedIntervalRecorded(/*expectedCnt*/ 1);
        assertImageFirstExpiredOpenFileRecorded(/*expectedCnt*/ 0);
        assertImageAllExpiredOpenFileRecorded(/*expectedCnt*/ 0);

        // #openFile is called from the drop target app with the expired uri.
        Assert.assertNull(
                "Previous uri should expire.", mDropDataContentProvider.openFile(uri, "r"));
        assertImageFirstExpiredOpenFileRecorded(/*expectedCnt*/ 1);
        assertImageAllExpiredOpenFileRecorded(/*expectedCnt*/ 1);

        // #openFile is called again from the drop target app with the expired uri.
        Assert.assertNull(
                "Previous uri should expire.", mDropDataContentProvider.openFile(uri, "r"));
        assertImageFirstExpiredOpenFileRecorded(/*expectedCnt*/ 1);
        assertImageAllExpiredOpenFileRecorded(/*expectedCnt*/ 2);

        ShadowLooper.idleMainLooper(CLEAR_CACHED_DATA_INTERVAL_MS, TimeUnit.MILLISECONDS);
        assertImageFirstOpenFileRecorded(/*expectedCnt*/ 1);
        // Android.DragDrop.Image.OpenFileTime.LastAttempt is not recorded because #clearCache is
        // cancelled by the second #cache.
        assertImageLastOpenFileRecorded(/*expectedCnt*/ 0);
    }

    private void assertImageSizeRecorded(int expectedCnt) {
        final String histogram = "Android.DragDrop.Image.Size";
        final String errorMsg = "<" + histogram + "> is not recorded properly.";
        Assert.assertEquals(
                errorMsg, expectedCnt, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private void assertImageUriCreatedIntervalRecorded(int expectedCnt) {
        final String histogram = "Android.DragDrop.Image.UriCreatedInterval";
        final String errorMsg = "<" + histogram + "> is not recorded properly.";
        Assert.assertEquals(
                errorMsg, expectedCnt, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private void assertImageFirstOpenFileRecorded(int expectedCnt) {
        final String histogram = "Android.DragDrop.Image.OpenFileTime.FirstAttempt";
        final String errorMsg = "<" + histogram + "> is not recorded properly.";
        Assert.assertEquals(
                errorMsg, expectedCnt, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private void assertImageLastOpenFileRecorded(int expectedCnt) {
        final String histogram = "Android.DragDrop.Image.OpenFileTime.LastAttempt";
        final String errorMsg = "<" + histogram + "> is not recorded properly.";
        Assert.assertEquals(
                errorMsg, expectedCnt, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private void assertImageFirstExpiredOpenFileRecorded(int expectedCnt) {
        final String histogram = "Android.DragDrop.Image.OpenFileTime.FirstExpired";
        final String errorMsg = "<" + histogram + "> is not recorded properly.";
        Assert.assertEquals(
                errorMsg, expectedCnt, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }

    private void assertImageAllExpiredOpenFileRecorded(int expectedCnt) {
        final String histogram = "Android.DragDrop.Image.OpenFileTime.AllExpired";
        final String errorMsg = "<" + histogram + "> is not recorded properly.";
        Assert.assertEquals(
                errorMsg, expectedCnt, RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }
}

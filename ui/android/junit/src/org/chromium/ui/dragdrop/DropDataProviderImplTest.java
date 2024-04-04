// Copyright 2022 The Chromium Authors
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
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.FileNotFoundException;
import java.util.concurrent.TimeUnit;

/** Test basic functionality of {@link DropDataProviderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DropDataProviderImplTest {
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

    private DropDataProviderImpl mDropDataProviderImpl;

    @Before
    public void setUp() {
        mDropDataProviderImpl = new DropDataProviderImpl();
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("jpg", "image/jpeg");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("gif", "image/gif");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("png", "image/png");
    }

    @After
    public void tearDown() {
        mDropDataProviderImpl.clearCache();
        mDropDataProviderImpl.clearLastUriCreatedTimestampForTesting();
    }

    @Test
    @SmallTest
    public void testCache() {
        Uri uri = mDropDataProviderImpl.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        Assert.assertEquals(
                "The MIME type for jpg file should be image/jpeg",
                "image/jpeg",
                mDropDataProviderImpl.getType(uri));
        assertImageSizeRecorded(/* expectedCnt= */ 1);
        // Android.DragDrop.Image.UriCreatedInterval is not recorded for the first created Uri.
        assertImageUriCreatedIntervalRecorded(/* expectedCnt= */ 0);

        uri = mDropDataProviderImpl.cache(IMAGE_DATA_B, EXTENSION_B, IMAGE_FILENAME_B);
        Assert.assertEquals(
                "The MIME type for gif file should be image/gif",
                "image/gif",
                mDropDataProviderImpl.getType(uri));
        assertImageSizeRecorded(/* expectedCnt= */ 2);
        assertImageUriCreatedIntervalRecorded(/* expectedCnt= */ 1);

        uri = mDropDataProviderImpl.cache(IMAGE_DATA_C, EXTENSION_C, IMAGE_FILENAME_C);
        Assert.assertEquals(
                "The MIME type for png file should be image/png",
                "image/png",
                mDropDataProviderImpl.getType(uri));
        assertImageSizeRecorded(/* expectedCnt= */ 3);
        assertImageUriCreatedIntervalRecorded(/* expectedCnt= */ 2);
    }

    @Test
    @SmallTest
    public void testGetStreamTypes() {
        Uri uri = mDropDataProviderImpl.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        String[] res = mDropDataProviderImpl.getStreamTypes(uri, "image/*");
        Assert.assertEquals("res length should be 1 when uri matches the filter", 1, res.length);
        Assert.assertEquals(
                "The MIME type for jpg file should be image/jpeg", "image/jpeg", res[0]);

        res = mDropDataProviderImpl.getStreamTypes(uri, "*/gif");
        Assert.assertNull("res should be null when uri does not match the filter", res);
    }

    @Test
    @SmallTest
    public void testQuery() {
        Uri uri = mDropDataProviderImpl.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        Cursor cursor = mDropDataProviderImpl.query(uri, null);
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
        Uri uri = mDropDataProviderImpl.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        mDropDataProviderImpl.onDragEnd(false);
        Assert.assertNull(
                "Image bytes should be null after clearing cache.",
                mDropDataProviderImpl.getImageBytesForTesting());
        Assert.assertNull(
                "Handler should be null after clearing cache.",
                mDropDataProviderImpl.getHandlerForTesting());
        Assert.assertNull(
                "MIME type should be null after clearing cache.",
                mDropDataProviderImpl.getType(uri));
    }

    @Test
    @SmallTest
    public void testClearCacheWithDelay() throws FileNotFoundException {
        Uri uri = mDropDataProviderImpl.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        mDropDataProviderImpl.setClearCachedDataIntervalMs(CLEAR_CACHED_DATA_INTERVAL_MS);
        ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
        // #openFile could be called before or after the Android Drag End event.
        mDropDataProviderImpl.openFile(new DropDataContentProvider(), uri);
        mDropDataProviderImpl.onDragEnd(true);
        Assert.assertNotNull(
                "Image bytes should not be null immediately after clear cache with delay.",
                mDropDataProviderImpl.getImageBytesForTesting());
        Assert.assertNotNull(
                "Handler should not be null after clear cache with delay.",
                mDropDataProviderImpl.getHandlerForTesting());
        Assert.assertEquals(
                "The MIME type for jpg file should be image/jpeg",
                "image/jpeg",
                mDropDataProviderImpl.getType(uri));
        assertImageFirstOpenFileRecorded(/* expectedCnt= */ 1);
        assertImageLastOpenFileRecorded(/* expectedCnt= */ 0);

        ShadowLooper.idleMainLooper(CLEAR_CACHED_DATA_INTERVAL_MS, TimeUnit.MILLISECONDS);
        Assert.assertNull(
                "Image bytes should be null after the delayed time.",
                mDropDataProviderImpl.getImageBytesForTesting());
        Assert.assertNull(
                "MIME type should be null after the delayed time.",
                mDropDataProviderImpl.getType(uri));
        assertImageFirstOpenFileRecorded(/* expectedCnt= */ 1);
        assertImageLastOpenFileRecorded(/* expectedCnt= */ 1);
    }

    @Test
    @SmallTest
    public void testClearCacheWithDelayCancelled() throws FileNotFoundException {
        Uri uri = mDropDataProviderImpl.cache(IMAGE_DATA_A, EXTENSION_A, IMAGE_FILENAME_A);
        mDropDataProviderImpl.setClearCachedDataIntervalMs(CLEAR_CACHED_DATA_INTERVAL_MS);
        mDropDataProviderImpl.onDragEnd(true);
        ShadowLooper.idleMainLooper(1, TimeUnit.MILLISECONDS);
        mDropDataProviderImpl.openFile(new DropDataContentProvider(), uri);
        // Android.DragDrop.Image.UriCreatedInterval is not recorded for the first created Uri.
        assertImageUriCreatedIntervalRecorded(/* expectedCnt= */ 0);

        // Next image drag starts before the previous image expires.
        mDropDataProviderImpl.cache(IMAGE_DATA_B, EXTENSION_B, IMAGE_FILENAME_B);
        assertImageUriCreatedIntervalRecorded(/* expectedCnt= */ 1);
        assertImageFirstExpiredOpenFileRecorded(/* expectedCnt= */ 0);
        assertImageAllExpiredOpenFileRecorded(/* expectedCnt= */ 0);

        // #openFile is called from the drop target app with the expired uri.
        Assert.assertNull(
                "Previous uri should expire.",
                mDropDataProviderImpl.openFile(new DropDataContentProvider(), uri));
        assertImageFirstExpiredOpenFileRecorded(/* expectedCnt= */ 1);
        assertImageAllExpiredOpenFileRecorded(/* expectedCnt= */ 1);

        // #openFile is called again from the drop target app with the expired uri.
        Assert.assertNull(
                "Previous uri should expire.",
                mDropDataProviderImpl.openFile(new DropDataContentProvider(), uri));
        assertImageFirstExpiredOpenFileRecorded(/* expectedCnt= */ 1);
        assertImageAllExpiredOpenFileRecorded(/* expectedCnt= */ 2);

        ShadowLooper.idleMainLooper(CLEAR_CACHED_DATA_INTERVAL_MS, TimeUnit.MILLISECONDS);
        assertImageFirstOpenFileRecorded(/* expectedCnt= */ 1);
        // Android.DragDrop.Image.OpenFileTime.LastAttempt is not recorded because #clearCache is
        // cancelled by the second #cache.
        assertImageLastOpenFileRecorded(/* expectedCnt= */ 0);
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

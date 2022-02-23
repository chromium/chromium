// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.robolectric.Shadows.shadowOf;

import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

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

    @Test
    @SmallTest
    public void testGetType() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A);
        Assert.assertEquals("The MIME type for jpg file should be image/jpeg", "image/jpeg",
                mDropDataContentProvider.getType(uri));
        uri = DropDataContentProvider.cache(IMAGE_DATA_B, EXTENSION_B);
        Assert.assertEquals("The MIME type for gif file should be image/gif", "image/gif",
                mDropDataContentProvider.getType(uri));
        uri = DropDataContentProvider.cache(IMAGE_DATA_C, EXTENSION_C);
        Assert.assertEquals("The MIME type for png file should be image/png", "image/png",
                mDropDataContentProvider.getType(uri));
    }

    @Test
    @SmallTest
    public void testGetStreamTypes() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A);
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
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A);
        Cursor cursor = mDropDataContentProvider.query(uri, null, null, null, null);
        Assert.assertEquals("The number of rows in the cursor should be 1", 1, cursor.getCount());
        Assert.assertEquals("The number of columns should be 2", 2, cursor.getColumnCount());
        cursor.moveToNext();
        int colIdx = cursor.getColumnIndex(OpenableColumns.SIZE);
        Assert.assertEquals("The file size should be 100", 100, cursor.getInt(colIdx));
        cursor.close();
    }

    @Test
    @SmallTest
    public void testClearCache() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A);
        DropDataContentProvider.clearCache();
        Assert.assertNull("Image bytes should be null after clearing cache.",
                DropDataContentProvider.getImageBytesForTesting());
        Assert.assertNull("Handler should be null after clearing cache.",
                DropDataContentProvider.getHandlerForTesting());
        Assert.assertNull("MIME type should be null after clearing cache.",
                mDropDataContentProvider.getType(uri));
    }

    @Test
    @SmallTest
    public void testClearCacheWithDelay() {
        Uri uri = DropDataContentProvider.cache(IMAGE_DATA_A, EXTENSION_A);
        DropDataContentProvider.setClearCachedDataIntervalMs(CLEAR_CACHED_DATA_INTERVAL_MS);
        DropDataContentProvider.clearCacheWithDelay();
        Assert.assertNotNull(
                "Image bytes should not be null immediately after clear cache with delay.",
                DropDataContentProvider.getImageBytesForTesting());
        Assert.assertNotNull("Handler should not be null after clear cache with delay.",
                DropDataContentProvider.getHandlerForTesting());
        Assert.assertEquals("The MIME type for jpg file should be image/jpeg", "image/jpeg",
                mDropDataContentProvider.getType(uri));
        ShadowLooper.idleMainLooper(CLEAR_CACHED_DATA_INTERVAL_MS + 1, TimeUnit.MILLISECONDS);
        Assert.assertNull("Image bytes should be null after the delayed time.",
                DropDataContentProvider.getImageBytesForTesting());
        Assert.assertNull("MIME type should be null after the delayed time.",
                mDropDataContentProvider.getType(uri));
    }
}

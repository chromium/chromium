// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.io.FileNotFoundException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/** Unit tests for {@link ClipboardTextDataProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ClipboardTextDataProviderUnitTest {
    private ContentResolver mContentResolver;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        Robolectric.setupContentProvider(
                ClipboardTextDataProvider.class,
                context.getPackageName() + ".ClipboardTextDataProvider");
        mContentResolver = context.getContentResolver();
    }

    @After
    public void tearDown() {
        ClipboardTextDataProvider.clearForTesting();
    }

    @Test
    public void testStoreAndReadTextOnly() throws Exception {
        String testText = "Large plain text content";
        Uri uri = ClipboardTextDataProvider.store(testText, null);
        assertNotNull(uri);
        assertEquals("content", uri.getScheme());
        assertNotNull(uri.getQueryParameter("uuid"));

        assertEquals(MimeTypeUtils.TEXT_PLAIN_MIME_TYPE, mContentResolver.getType(uri));
        assertArrayEquals(
                new String[] {MimeTypeUtils.TEXT_PLAIN_MIME_TYPE},
                mContentResolver.getStreamTypes(uri, MimeTypeUtils.TEXT_PLAIN_MIME_TYPE));
        assertNull(mContentResolver.getStreamTypes(uri, MimeTypeUtils.TEXT_HTML_MIME_TYPE));

        String readContent = readContentFromUri(uri, MimeTypeUtils.TEXT_PLAIN_MIME_TYPE);
        assertEquals(testText, readContent);
    }

    @Test
    public void testStoreAndReadHtmlOnly() throws Exception {
        String testHtml = "<h1>Large html content</h1>";
        Uri uri = ClipboardTextDataProvider.store(null, testHtml);
        assertNotNull(uri);
        assertNotNull(uri.getQueryParameter("uuid"));

        assertEquals(MimeTypeUtils.TEXT_HTML_MIME_TYPE, mContentResolver.getType(uri));
        assertArrayEquals(
                new String[] {MimeTypeUtils.TEXT_HTML_MIME_TYPE},
                mContentResolver.getStreamTypes(uri, MimeTypeUtils.TEXT_HTML_MIME_TYPE));
        assertNull(mContentResolver.getStreamTypes(uri, MimeTypeUtils.TEXT_PLAIN_MIME_TYPE));

        String readContent = readContentFromUri(uri, MimeTypeUtils.TEXT_HTML_MIME_TYPE);
        assertEquals(testHtml, readContent);
    }

    @Test
    public void testStoreAndReadBothTextAndHtml() throws Exception {
        String testText = "Plain text";
        String testHtml = "<p>Plain text</p>";
        Uri uri = ClipboardTextDataProvider.store(testText, testHtml);
        assertNotNull(uri);

        assertArrayEquals(
                new String[] {MimeTypeUtils.TEXT_PLAIN_MIME_TYPE},
                mContentResolver.getStreamTypes(uri, MimeTypeUtils.TEXT_PLAIN_MIME_TYPE));
        assertArrayEquals(
                new String[] {MimeTypeUtils.TEXT_HTML_MIME_TYPE},
                mContentResolver.getStreamTypes(uri, MimeTypeUtils.TEXT_HTML_MIME_TYPE));
        String[] allTypes =
                mContentResolver.getStreamTypes(uri, MimeTypeUtils.ALL_FILE_TYPES_MIME_TYPE);
        assertNotNull(allTypes);
        assertEquals(2, allTypes.length);

        assertEquals(testText, readContentFromUri(uri, MimeTypeUtils.TEXT_PLAIN_MIME_TYPE));
        assertEquals(testHtml, readContentFromUri(uri, MimeTypeUtils.TEXT_HTML_MIME_TYPE));
    }

    @Test
    public void testStoreAndReadPreservesNewlines() throws Exception {
        String multiLineText = "Line 1\nLine 2\r\nLine 3\n\nLine 4";
        String multiLineHtml = "<div>\n  <p>First paragraph</p>\n  <p>Second paragraph</p>\n</div>";
        Uri uri = ClipboardTextDataProvider.store(multiLineText, multiLineHtml);
        assertNotNull(uri);

        assertEquals(multiLineText, readContentFromUri(uri, MimeTypeUtils.TEXT_PLAIN_MIME_TYPE));
        assertEquals(multiLineHtml, readContentFromUri(uri, MimeTypeUtils.TEXT_HTML_MIME_TYPE));
    }

    @Test(expected = FileNotFoundException.class)
    public void testInvalidUuidQueryParameterRejected() throws Exception {
        Uri validUri = ClipboardTextDataProvider.store("text", null);
        assertNotNull(validUri);

        Uri tamperedUri =
                validUri.buildUpon()
                        .clearQuery()
                        .appendQueryParameter("uuid", "00000000-0000-0000-0000-000000000000")
                        .build();

        assertNull(mContentResolver.getType(tamperedUri));
        assertNull(
                mContentResolver.getStreamTypes(
                        tamperedUri, MimeTypeUtils.ALL_FILE_TYPES_MIME_TYPE));

        mContentResolver.openTypedAssetFileDescriptor(
                tamperedUri, MimeTypeUtils.TEXT_PLAIN_MIME_TYPE, null);
    }

    @Test
    public void testQueryExposesNoOpenableColumns() {
        Uri uri = ClipboardTextDataProvider.store("text", "html");
        assertNotNull(uri);

        try (Cursor cursor = mContentResolver.query(uri, null, null, null, null)) {
            assertNotNull(cursor);
            assertEquals(-1, cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
            assertEquals(-1, cursor.getColumnIndex(OpenableColumns.SIZE));
        }
    }

    @Test
    public void testStoreNullReturnsNull() {
        assertNull(ClipboardTextDataProvider.store(null, null));
    }

    private String readContentFromUri(Uri uri, String mimeType) throws Exception {
        try (AssetFileDescriptor afd =
                mContentResolver.openTypedAssetFileDescriptor(uri, mimeType, null)) {
            assertNotNull(afd);
            RobolectricUtil.runAllBackgroundAndUi();
            try (InputStream is = afd.createInputStream()) {
                return new String(is.readAllBytes(), StandardCharsets.UTF_8);
            }
        }
    }
}

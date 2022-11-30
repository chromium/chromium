// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Intent;
import android.net.Uri;
import android.text.SpannableString;
import android.text.style.RelativeSizeSpan;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.StreamUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Tests logic in the Clipboard class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClipboardTest {
    private static final String PLAIN_TEXT = "plain";
    private static final String HTML_TEXT = "<span style=\"color: red;\">HTML</span>";
    private static final byte[] TEST_IMAGE_DATA = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    private Uri generateUriFromImage(final byte[] jpegImageData) {
        FileOutputStream fOut = null;
        try {
            File path = new File(UrlUtils.getIsolatedTestFilePath("test_image"));
            if (path.exists() || path.mkdir()) {
                File saveFile = File.createTempFile(
                        String.valueOf(System.currentTimeMillis()), ".jpg", path);
                fOut = new FileOutputStream(saveFile);
                fOut.write(jpegImageData);
                fOut.flush();

                return ContentUriUtils.getContentUriFromFile(saveFile);
            }
        } catch (IOException ie) {
            // Ignore exception.
        } finally {
            StreamUtil.closeQuietly(fOut);
        }

        return null;
    }

    @Test
    public void testClipDataToHtmlText() {
        Clipboard clipboard = Clipboard.getInstance();

        // HTML text
        ClipData html = ClipData.newHtmlText("html", PLAIN_TEXT, HTML_TEXT);
        assertEquals(HTML_TEXT, clipboard.clipDataToHtmlText(html));

        // Plain text without span
        ClipData plainTextNoSpan = ClipData.newPlainText("plain", PLAIN_TEXT);
        assertNull(clipboard.clipDataToHtmlText(plainTextNoSpan));

        // Plain text with span
        SpannableString spanned = new SpannableString(PLAIN_TEXT);
        spanned.setSpan(new RelativeSizeSpan(2f), 0, 5, 0);
        ClipData plainTextSpan = ClipData.newPlainText("plain", spanned);
        assertNotNull(clipboard.clipDataToHtmlText(plainTextSpan));

        // Intent
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        ClipData intentClip = ClipData.newIntent("intent", intent);
        assertNull(clipboard.clipDataToHtmlText(intentClip));
    }

    @Test
    public void testClipboardSetImage() {
        Clipboard clipboard = Clipboard.getInstance();

        // simple set a null, check if there is no crash.
        clipboard.setImageUri(null);

        // Set actually data.
        Uri imageUri = generateUriFromImage(TEST_IMAGE_DATA);
        clipboard.setImageUri(imageUri);

        assertEquals(imageUri, clipboard.getImageUri());
    }

    @Test
    public void testClipboardCopyUrlToClipboard() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = Mockito.mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);

        String url = JUnitTestGURLs.SEARCH_URL;
        clipboard.copyUrlToClipboard(JUnitTestGURLs.getGURL(url));

        ArgumentCaptor<ClipData> clipCaptor = ArgumentCaptor.forClass(ClipData.class);
        verify(clipboardManager).setPrimaryClip(clipCaptor.capture());
        assertEquals("url", clipCaptor.getValue().getDescription().getLabel());
        assertEquals(url, clipCaptor.getValue().getItemAt(0).getText());
    }

    @Test
    public void testClipboardCopyUrlToClipboardNoException() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = Mockito.mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);

        doThrow(SecurityException.class).when(clipboardManager).setPrimaryClip(any(ClipData.class));
        String url = JUnitTestGURLs.SEARCH_URL;
        clipboard.copyUrlToClipboard(JUnitTestGURLs.getGURL(url));

        ArgumentCaptor<ClipData> clipCaptor = ArgumentCaptor.forClass(ClipData.class);
        verify(clipboardManager).setPrimaryClip(clipCaptor.capture());
        assertEquals("url", clipCaptor.getValue().getDescription().getLabel());
        assertEquals(url, clipCaptor.getValue().getItemAt(0).getText());
    }

    @Test
    public void testHasCoercedTextCanGetUrl() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = Mockito.mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);

        ClipDescription clipDescription =
                new ClipDescription("url", new String[] {"text/x-moz-url"});
        when(clipboardManager.getPrimaryClipDescription()).thenReturn(clipDescription);

        assertTrue(clipboard.hasCoercedText());
    }
}

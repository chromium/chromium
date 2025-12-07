// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.text.SpannableString;
import android.text.style.RelativeSizeSpan;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.widget.ToastManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests logic in the Clipboard class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class})
public class ClipboardTest {
    private static final String PLAIN_TEXT = "plain";
    private static final String HTML_TEXT = "<span style=\"color: red;\">HTML</span>";
    private Uri mTempImageUri;

    @Before
    public void setup() {
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(int taskTraits, Runnable task, long delay) {
                        new Handler(Looper.getMainLooper()).postDelayed(task, delay);
                    }
                });
        mTempImageUri = Uri.parse("content://tmp/test/image.jpg");
        ClipboardImpl.setSkipImageMimeTypeCheckForTesting(true);
    }

    @After
    public void tearDown() {
        ToastManager.resetForTesting();
        ShadowToast.reset();
        Clipboard.resetForTesting();
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
        ShadowLooper.idleMainLooper();
        assertNull(clipboard.getImageUri());

        // Set actually data.
        clipboard.setImageUri(mTempImageUri);
        ShadowLooper.idleMainLooper();
        assertEquals(mTempImageUri, clipboard.getImageUri());
    }

    @Test
    public void testClipboardCopyUrlToClipboard() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = Mockito.mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);

        GURL url = JUnitTestGURLs.SEARCH_URL;
        clipboard.copyUrlToClipboard(url);

        ArgumentCaptor<ClipData> clipCaptor = ArgumentCaptor.forClass(ClipData.class);
        verify(clipboardManager).setPrimaryClip(clipCaptor.capture());
        assertEquals("url", clipCaptor.getValue().getDescription().getLabel());
        assertEquals(url.getSpec(), clipCaptor.getValue().getItemAt(0).getText());
    }

    @Test
    public void testClipboardCopyUrlToClipboardNoException() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = Mockito.mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);

        doThrow(SecurityException.class).when(clipboardManager).setPrimaryClip(any(ClipData.class));
        GURL url = JUnitTestGURLs.SEARCH_URL;
        clipboard.copyUrlToClipboard(url);

        ArgumentCaptor<ClipData> clipCaptor = ArgumentCaptor.forClass(ClipData.class);
        verify(clipboardManager).setPrimaryClip(clipCaptor.capture());
        assertEquals("url", clipCaptor.getValue().getDescription().getLabel());
        assertEquals(url.getSpec(), clipCaptor.getValue().getItemAt(0).getText());
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

    @Test
    public void testClipboardGetFilenames() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = Mockito.mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);

        ClipData clipData = ClipData.newPlainText("label", "text");
        when(clipboardManager.getPrimaryClip()).thenReturn(clipData);
        assertFalse(clipboard.hasFilenames());
        assertEquals(0, clipboard.getFilenames().length);

        ContentResolver cr = ContextUtils.getApplicationContext().getContentResolver();
        String file1 = "content://tmp/test/file1.jpg";
        String file2 = "content://tmp/test/file2.txt";
        clipData = ClipData.newUri(cr, "label", Uri.parse(file1));
        when(clipboardManager.getPrimaryClip()).thenReturn(clipData);
        assertTrue(clipboard.hasFilenames());
        String[][] filenames = clipboard.getFilenames();
        assertEquals(1, filenames.length);
        assertEquals(2, filenames[0].length);
        assertEquals("content://tmp/test/file1.jpg", filenames[0][0]);
        assertEquals("", filenames[0][1]);

        clipData = ClipData.newPlainText("label", "text");
        clipData.addItem(new ClipData.Item(Uri.parse(file1)));
        clipData.addItem(new ClipData.Item(Uri.parse(file2)));
        when(clipboardManager.getPrimaryClip()).thenReturn(clipData);
        assertTrue(clipboard.hasFilenames());
        filenames = clipboard.getFilenames();
        assertEquals(2, filenames.length);
        assertEquals(2, filenames[0].length);
        assertEquals("content://tmp/test/file1.jpg", filenames[0][0]);
        assertEquals("", filenames[0][1]);
        assertEquals(2, filenames[1].length);
        assertEquals("content://tmp/test/file2.txt", filenames[1][0]);
        assertEquals("", filenames[1][1]);
    }

    @Test
    public void testClipboardSetFilenames() {
        Clipboard clipboard = Clipboard.getInstance();
        ClipboardManager clipboardManager = Mockito.mock(ClipboardManager.class);
        ((ClipboardImpl) clipboard).overrideClipboardManagerForTesting(clipboardManager);

        String file1 = "content://tmp/test/file1.jpg";
        String file2 = "content://tmp/test/file2.txt";
        clipboard.setFilenames(new String[] {file1, file2});

        ArgumentCaptor<ClipData> clipCaptor = ArgumentCaptor.forClass(ClipData.class);
        verify(clipboardManager).setPrimaryClip(clipCaptor.capture());
        assertEquals(2, clipCaptor.getValue().getItemCount());
        assertEquals(file1, clipCaptor.getValue().getItemAt(0).getUri().toString());
        assertEquals(file2, clipCaptor.getValue().getItemAt(1).getUri().toString());
    }

    @Test
    @Config(shadows = ShadowToast.class)
    public void setTextWithNotification() {
        Clipboard.getInstance().setText("label", "text", false);
        assertNull(ShadowToast.getLatestToast());

        Clipboard.getInstance().setText("label", "text", true);
        assertNotNull(ShadowToast.getLatestToast());
        assertTextFromLatestToast(R.string.copied);
    }

    @Test
    @Config(shadows = ShadowToast.class)
    public void setImageWithNotification() {
        Clipboard.getInstance().setImageUri(mTempImageUri, false);
        ShadowLooper.idleMainLooper();
        assertNull(ShadowToast.getLatestToast());

        Clipboard.getInstance().setImageUri(mTempImageUri, true);
        ShadowLooper.idleMainLooper();
        assertNotNull(ShadowToast.getLatestToast());
        assertTextFromLatestToast(R.string.image_copied);
    }

    @Test
    @Config(shadows = ShadowToast.class)
    public void setImageWithFailedNotification() {
        Clipboard.getInstance().setImageUri(null, false);
        ShadowLooper.idleMainLooper();
        assertNotNull(ShadowToast.getLatestToast());
        assertTextFromLatestToast(R.string.copy_to_clipboard_failure_message);
    }

    private void assertTextFromLatestToast(@StringRes int strRes) {
        TextView textView = (TextView) ShadowToast.getLatestToast().getView();
        String actualText = textView == null ? "" : textView.getText().toString();

        assertEquals(
                "Text for toast shown does not match.",
                ContextUtils.getApplicationContext().getString(strRes),
                actualText);
    }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.ClipData;
import android.content.Intent;
import android.text.SpannableString;
import android.text.style.RelativeSizeSpan;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests logic in the Clipboard class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClipboardTest {
    private static final String PLAIN_TEXT = "plain";
    private static final String HTML_TEXT = "<span style=\"color: red;\">HTML</span>";

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
}

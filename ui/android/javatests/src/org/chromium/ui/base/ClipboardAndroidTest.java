// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Clipboard tests for Android platform that depend on access to the ClipboardManager.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ClipboardAndroidTest extends DummyUiActivityTestCase {
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
    }

    @Override
    public void tearDownTest() throws Exception {
        ClipboardAndroidTestSupport.cleanup();
        super.tearDownTest();
    }

    /**
     * Test that if another application writes some text to the pasteboard the clipboard properly
     * invalidates other types.
     */
    @Test
    @SmallTest
    public void internalClipboardInvalidation() {
        // Write to the clipboard in native and ensure that is propagated to the platform clipboard.
        final String originalText = "foo";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Original text was not written to the native clipboard.",
                    ClipboardAndroidTestSupport.writeHtml(originalText));
        });

        // Assert that the ClipboardManager contains the original text. Then simulate another
        // application writing to the clipboard.
        final String invalidatingText = "Hello, World!";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClipboardManager clipboardManager =
                    (ClipboardManager) getActivity().getSystemService(Context.CLIPBOARD_SERVICE);

            Assert.assertEquals("Original text not found in ClipboardManager.", originalText,
                    Clipboard.getInstance().clipDataToHtmlText(clipboardManager.getPrimaryClip()));

            clipboardManager.setPrimaryClip(ClipData.newPlainText(null, invalidatingText));
        });

        // Assert that the overwrite from another application is registered by the native clipboard.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Invalidating text not found in the native clipboard.",
                    ClipboardAndroidTestSupport.clipboardContains(invalidatingText));
        });
    }
}

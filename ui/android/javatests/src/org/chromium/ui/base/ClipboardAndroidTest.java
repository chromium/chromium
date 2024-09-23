// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.concurrent.TimeoutException;

/**
 * Clipboard tests for Android platform that depend on access to the ClipboardManager.
 *
 * This test suite can fail on Android 10+ if the activity does not maintain focus during testing.
 * For more information see: https://crbug.com/1297678 and
 * https://developer.android.com/about/versions/10/privacy/changes#clipboard-data
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ClipboardAndroidTest extends BlankUiTestActivityTestCase {
    private static final String TEXT_URL = "http://www.foo.com/";
    private static final String MIX_TEXT_URL = "test http://www.foo.com http://www.bar.com";
    private static final String MIX_TEXT_URL_NO_PROTOCOL = "test www.foo.com www.bar.com";

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Override
    public void tearDownTest() throws Exception {
        Clipboard.cleanupNativeForTesting();

        // Clear the clipboard to avoid leaving any state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboardManager =
                            (ClipboardManager)
                                    getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clipData = ClipData.newPlainText("", "");
                    clipboardManager.setPrimaryClip(clipData);
                });
        super.tearDownTest();
    }

    /**
     * Test that if another application writes some text to the pasteboard the clipboard properly
     * invalidates other types.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1413839")
    public void internalClipboardInvalidation() throws TimeoutException {
        // Write to the clipboard in native and ensure that is propagated to the platform clipboard.
        final String originalText = "foo";
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Original text was not written to the native clipboard.",
                            ClipboardAndroidTestSupport.writeHtml(originalText));
                });

        CallbackHelper helper = new CallbackHelper();
        ClipboardManager.OnPrimaryClipChangedListener clipboardChangedListener =
                new ClipboardManager.OnPrimaryClipChangedListener() {
                    @Override
                    public void onPrimaryClipChanged() {
                        helper.notifyCalled();
                    }
                };

        // Assert that the ClipboardManager contains the original text. Then simulate another
        // application writing to the clipboard.
        final String invalidatingText = "Hello, World!";
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboardManager =
                            (ClipboardManager)
                                    getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
                    clipboardManager.addPrimaryClipChangedListener(clipboardChangedListener);

                    Assert.assertEquals(
                            "Original text not found in ClipboardManager.",
                            originalText,
                            Clipboard.getInstance()
                                    .clipDataToHtmlText(clipboardManager.getPrimaryClip()));

                    clipboardManager.setPrimaryClip(ClipData.newPlainText(null, invalidatingText));
                });

        helper.waitForCallback("ClipboardManager did not notify of PrimaryClip change.", 0);

        // Assert that the overwrite from another application is registered by the native clipboard.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Invalidating text not found in the native clipboard.",
                            ClipboardAndroidTestSupport.clipboardContains(invalidatingText));

                    ClipboardManager clipboardManager =
                            (ClipboardManager)
                                    getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
                    clipboardManager.removePrimaryClipChangedListener(clipboardChangedListener);
                });
    }

    @Test
    @SmallTest
    public void hasHTMLOrStyledTextForNormalTextTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Clipboard.getInstance().setText("SampleTextToCopy");
                    Assert.assertFalse(Clipboard.getInstance().hasHTMLOrStyledText());
                });
    }

    @Test
    @SmallTest
    public void hasHTMLOrStyledTextForStyledTextTest() {
        SpannableString spanString = new SpannableString("SpannableString");
        spanString.setSpan(new BackgroundColorSpan(0), 0, 4, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        ClipData clipData =
                ClipData.newPlainText("text", spanString.subSequence(0, spanString.length() - 1));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((ClipboardImpl) Clipboard.getInstance()).setPrimaryClipNoException(clipData);
                    Assert.assertTrue(Clipboard.getInstance().hasHTMLOrStyledText());
                });
    }

    @Test
    @SmallTest
    public void hasHTMLOrStyledTextForHtmlTextTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Clipboard.getInstance()
                            .setHTMLText(
                                    "<span style=\"color: red;\">HTMLTextToCopy</span>",
                                    "HTMLTextToCopy");
                    Assert.assertTrue(Clipboard.getInstance().hasHTMLOrStyledText());
                });
    }

    @Test
    @SmallTest
    public void hasUrlAndGetUrlTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Clipboard.getInstance().setText(TEXT_URL);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(Clipboard.getInstance().hasUrl(), Matchers.is(true));
                    Criteria.checkThat(Clipboard.getInstance().getUrl(), Matchers.is(TEXT_URL));
                });
    }

    // Only first URL is returned on S+ if clipboard contains multiple URLs.
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    public void hasUrlAndGetUrlMixTextAndLinkTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Clipboard.getInstance().setText(MIX_TEXT_URL);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(Clipboard.getInstance().hasUrl(), Matchers.is(true));
                    Criteria.checkThat(Clipboard.getInstance().getUrl(), Matchers.is(TEXT_URL));
                });
    }

    // Only first URL is returned on S+ if clipboard contains multiple URLs.
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    public void hasUrlAndGetUrlMixTextAndLinkWithoutProtocolTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Clipboard.getInstance().setText(MIX_TEXT_URL_NO_PROTOCOL);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(Clipboard.getInstance().hasUrl(), Matchers.is(true));
                    Criteria.checkThat(Clipboard.getInstance().getUrl(), Matchers.is(TEXT_URL));
                });
    }
}

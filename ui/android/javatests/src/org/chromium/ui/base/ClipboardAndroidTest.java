// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.net.Uri;
import android.os.Build;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/**
 * Clipboard tests for Android platform that depend on access to the ClipboardManager.
 *
 * <p>This test suite can fail on Android 10+ if the activity does not maintain focus during
 * testing. For more information see: https://crbug.com/1297678 and
 * https://developer.android.com/about/versions/10/privacy/changes#clipboard-data
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@DisableIf.Build(
        sdk_is_greater_than = Build.VERSION_CODES.R,
        sdk_is_less_than = Build.VERSION_CODES.TIRAMISU,
        message = "crbug.com/1297678")
public class ClipboardAndroidTest {
    private static final String TEXT_URL = "http://www.foo.com/";
    private static final String MIX_TEXT_URL = "test http://www.foo.com http://www.bar.com";
    private static final String MIX_TEXT_URL_NO_PROTOCOL = "test www.foo.com www.bar.com";

    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PackageManager mMockPm;
    @Mock private Context mMockContext;
    @Mock private ClipDescription mMockClipDescription;
    @Mock private ClipboardManager mMockClipboardManager;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() throws Exception {
        Clipboard.cleanupNativeForTesting();

        // Clear the clipboard to avoid leaving any state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClipboardManager clipboardManager =
                            (ClipboardManager)
                                    sActivity.getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clipData = ClipData.newPlainText("", "");
                    clipboardManager.setPrimaryClip(clipData);
                });
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
                                    sActivity.getSystemService(Context.CLIPBOARD_SERVICE);
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
                                    sActivity.getSystemService(Context.CLIPBOARD_SERVICE);
                    clipboardManager.removePrimaryClipChangedListener(clipboardChangedListener);
                });
    }

    /**
     * Taking ownership of the clipboard bumps the native sequence number synchronously, and Android
     * then delivers an asynchronous onPrimaryClipChanged echo for that same write. That echo must
     * NOT bump the sequence number a second time, otherwise a listener that captures the sequence
     * number synchronously with respect to the write would observe a false divergence. A genuine
     * foreign change, which carries a newer timestamp, must still bump the sequence number.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @DisabledTest(message = "crbug.com/555727186")
    public void selfWriteClipChangedEchoDoesNotBumpSequenceNumber() {
        ClipboardImpl clipboard = (ClipboardImpl) Clipboard.getInstance();

        // Install a mock ClipboardManager so the primary clip's timestamp can be controlled
        // deterministically relative to the native last-modified time.
        when(mMockClipboardManager.getPrimaryClipDescription()).thenReturn(mMockClipDescription);
        ClipboardManager originalClipboardManager =
                clipboard.overrideClipboardManagerForTesting(mMockClipboardManager);

        try {
            // A native write bumps the sequence number synchronously and records its time as the
            // last-modified time.
            Assert.assertTrue(ClipboardAndroidTestSupport.writeHtml("foo"));
            long lastModifiedMs = clipboard.getLastModifiedTimeMs();
            String seqAfterWrite = ClipboardAndroidTestSupport.getSequenceNumber();

            // Android echoes our own write back as onPrimaryClipChanged carrying the same timestamp
            // we just recorded. That echo must be swallowed and must NOT bump the sequence number
            // again.
            when(mMockClipDescription.getTimestamp()).thenReturn(lastModifiedMs);
            clipboard.onPrimaryClipChanged();
            Assert.assertEquals(
                    "The echo of our own write must not bump the sequence number.",
                    seqAfterWrite,
                    ClipboardAndroidTestSupport.getSequenceNumber());

            // A genuine foreign change carries a newer timestamp and must bump the sequence number.
            when(mMockClipDescription.getTimestamp()).thenReturn(lastModifiedMs + 100000);
            clipboard.onPrimaryClipChanged();
            Assert.assertNotEquals(
                    "A foreign clipboard change must bump the sequence number.",
                    seqAfterWrite,
                    ClipboardAndroidTestSupport.getSequenceNumber());
        } finally {
            clipboard.overrideClipboardManagerForTesting(originalClipboardManager);
        }
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
    @DisabledTest(message = "https://crbug.com/383804517")
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
    @DisabledTest(message = "crbug.com/402756726")
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
    @DisabledTest(message = "crbug.com/382555273")
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

    @Test
    @SmallTest
    public void testNativeWriteToClipboardFiresNativeNotification() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Native write to clipboard should trigger change notifications",
                            ClipboardAndroidTestSupport.testNativeClipboardNotifications());
                });
    }

    @Test
    @SmallTest
    @EnableFeatures(UiAndroidFeatures.CLIPBOARD_CONFUSED_DEPUTY_DEFENSE_IMAGES)
    public void testConfusedDeputyDefenseForImages() {
        Context appContext = sActivity.getApplicationContext();
        ClipboardManager realClipboardManager =
                (ClipboardManager) appContext.getSystemService(Context.CLIPBOARD_SERVICE);
        when(mMockContext.getSystemService(Context.CLIPBOARD_SERVICE))
                .thenReturn(realClipboardManager);
        when(mMockContext.getPackageName()).thenReturn(appContext.getPackageName());

        ProviderInfo info = new ProviderInfo();
        info.packageName = appContext.getPackageName();
        when(mMockPm.resolveContentProvider(any(), anyInt())).thenReturn(info);
        when(mMockContext.getPackageManager()).thenReturn(mMockPm);

        ContextUtils.initApplicationContextForTests(mMockContext);

        Clipboard.resetForTesting();
        ClipboardImpl clipboard = (ClipboardImpl) Clipboard.getInstance();
        clipboard.setImageFileProvider(null); // Simulates external copy.

        ClipData clipData =
                new ClipData(
                        "image",
                        new String[] {"image/png"},
                        new ClipData.Item(Uri.parse("content://any/path")));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    clipboard.setPrimaryClipNoException(clipData);
                });
        Assert.assertNull(
                "Paste of own-app URI from malicious app should be rejected", clipboard.getPng());
    }
}

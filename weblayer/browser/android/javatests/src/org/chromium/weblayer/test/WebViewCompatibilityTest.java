// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Bundle;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests for compatibility with running WebView and WebLayer in the same process. These tests only
 * make sense when WebView and WebLayer are both being loaded from the same APK.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class WebViewCompatibilityTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    @DisabledTest(message = "http://crbug.com/1273417") // Failing on android-arm64-proguard-rel
    public void testBothLoadPageWebLayerFirst() throws Exception {
        mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestDataURL("simple_page.html"));

        loadPageWithWebView(mActivityTestRule.getTestDataURL("simple_page2.html"));

        // Make sure WebLayer still loads.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("simple_page3.html"));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "http://crbug.com/1273417") // Failing on android-arm64-proguard-rel
    public void testBothLoadPageWebViewFirst() throws Exception {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_CREATE_WEBLAYER, false);
        InstrumentationActivity activity = mActivityTestRule.launchShell(extras);

        loadPageWithWebView(mActivityTestRule.getTestDataURL("simple_page.html"));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.loadWebLayerSync(activity.getApplicationContext()));
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("simple_page2.html"));

        // Make sure WebView still loads.
        loadPageWithWebView(mActivityTestRule.getTestDataURL("simple_page3.html"));
    }

    private void loadPageWithWebView(String urlToLoad) throws Exception {
        WebView webView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Loading WebView triggers loading from disk.
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                return new WebView(mActivityTestRule.getActivity());
            }
        });
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            webView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView view, String url) {
                    Assert.assertEquals(url, urlToLoad);
                    callbackHelper.notifyCalled();
                }
            });
            webView.loadUrl(urlToLoad);
        });
        callbackHelper.waitForFirst();
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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
    public void testBothLoadPage() throws Exception {
        mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestDataURL("simple_page.html"));
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
                    callbackHelper.notifyCalled();
                }
            });
            webView.loadUrl(mActivityTestRule.getTestDataURL("simple_page2.html"));
        });
        callbackHelper.waitForFirst();
    }
}

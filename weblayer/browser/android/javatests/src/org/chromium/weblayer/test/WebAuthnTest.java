// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertEquals;

import android.os.Build;
import android.os.RemoteException;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.concurrent.TimeoutException;

/**
 * Tests WebLayer's implementation of the WebAuthn API.
 */
@CommandLineFlags.
Add({ContentSwitches.HOST_RESOLVER_RULES + "=\"MAP * 127.0.0.1\"", "ignore-certificate-errors"})
@RunWith(WebLayerJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.N) // WebAuthn is only supported on Android N+
public class WebAuthnTest {
    // Should match the domain specified in authenticator.html
    private static final String TEST_DOMAIN = "subdomain.example.test";
    private static final String TEST_FILE = "/content/test/data/android/authenticator.html";

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    private InstrumentationActivity mActivity;

    private static class TitleWatcher extends TabCallback {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();
        private String mTitle;

        @Override
        public void onTitleUpdated(String title) {
            mTitle = title;
            mCallbackHelper.notifyCalled();
        }

        public String waitForTitleChange() throws TimeoutException {
            if (mTitle == null) {
                mCallbackHelper.waitForCallback(mCallbackHelper.getCallCount());
            }
            return mTitle;
        }
    }

    @Before
    public void setUp() throws RemoteException {
        mActivityTestRule.getTestServerRule().setServerUsesHttps(true);
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestWebLayer.getTestWebLayer(mActivity.getApplicationContext())
                .setMockWebAuthnEnabled(true);
    }

    @Test
    @MediumTest
    public void testCreatePublicKeyCredential() throws Exception {
        String url = mActivityTestRule.getTestServer().getURLWithHostName(TEST_DOMAIN, TEST_FILE);
        mActivityTestRule.navigateAndWait(url);
        TitleWatcher titleWatcher = new TitleWatcher();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().registerTabCallback(titleWatcher);
            mActivity.getTab().executeScript(
                    "doCreatePublicKeyCredential()", false /* useSeparateIsolate */, null);
        });

        String title = titleWatcher.waitForTitleChange();
        assertEquals("Success", title);
    }
}

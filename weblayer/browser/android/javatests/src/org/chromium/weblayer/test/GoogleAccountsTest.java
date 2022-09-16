// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.GoogleAccountServiceType;
import org.chromium.weblayer.GoogleAccountsCallback;
import org.chromium.weblayer.GoogleAccountsParams;
import org.chromium.weblayer.shell.InstrumentationActivity;
;

/**
 * Tests for the Google accounts API.
 */
@CommandLineFlags.Add({"ignore-certificate-errors"})
@RunWith(WebLayerJUnit4ClassRunner.class)
public class GoogleAccountsTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.getTestServerRule().setServerUsesHttps(true);

        // We need to add this to the command line directly instead of using @CommandLineFlags.Add
        // since it uses the test server URL which is not available for the annotatoin.
        CommandLine.getInstance().appendSwitchWithValue(
                "gaia-url", mActivityTestRule.getTestServer().getURL("/"));
        mActivityTestRule.writeCommandLineFile();
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
    }

    @Test
    @SmallTest
    public void testBasic() throws Exception {
        GoogleAccountsCallbackImpl callback = new GoogleAccountsCallbackImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getBrowser().getActiveTab().setGoogleAccountsCallback(callback);
        });

        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("google_accounts.html"));
        GoogleAccountsParams params = callback.waitForGoogleAccounts();
        Assert.assertEquals(params.serviceType, GoogleAccountServiceType.ADD_SESSION);
        Assert.assertEquals(params.email, "foo@bar.com");
        Assert.assertEquals(params.continueUri.toString(), "https://blah.com");
        Assert.assertTrue(params.isSameTab);
    }

    @Test
    @SmallTest
    public void testRequestHeader() throws Exception {
        GoogleAccountsCallbackImpl callback = new GoogleAccountsCallbackImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getBrowser().getActiveTab().setGoogleAccountsCallback(callback);
        });

        String url = mActivityTestRule.getTestServer().getURL("/echoheader?X-Chrome-Connected");
        mActivityTestRule.navigateAndWait(url);
        Assert.assertEquals(
                mActivityTestRule.executeScriptAndExtractString("document.body.innerText"),
                "source=WebLayer,mode=3,enable_account_consistency=true,"
                        + "consistency_enabled_by_default=false");

        // Remove the callback, the header should no longer be sent.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowser().getActiveTab().setGoogleAccountsCallback(null); });
        mActivityTestRule.navigateAndWait(url);
        Assert.assertEquals(
                mActivityTestRule.executeScriptAndExtractString("document.body.innerText"), "None");
    }

    private static class GoogleAccountsCallbackImpl extends GoogleAccountsCallback {
        private CallbackHelper mHelper = new CallbackHelper();
        private GoogleAccountsParams mParams;

        @Override
        public void onGoogleAccountsRequest(GoogleAccountsParams params) {
            mParams = params;
            mHelper.notifyCalled();
        }

        @Override
        public String getGaiaId() {
            return "";
        }

        public GoogleAccountsParams waitForGoogleAccounts() throws Exception {
            mHelper.waitForFirst();
            return mParams;
        }
    }
}

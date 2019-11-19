// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.ErrorPageCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that ErrorPageCallback works as expected for handling error page interactions.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ErrorPageCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    private InstrumentationActivity mActivity;
    // Only one EmbeddedTestServer may be used at a time.
    private TestWebServer mGoodServer;
    private EmbeddedTestServer mBadSslServer;
    private String mGoodUrl;
    private String mBadUrl;
    private Callback mCallback;

    private static class Callback extends ErrorPageCallback {
        public boolean mSignaled;
        public String mSafetyPage;
        public Tab mTab;

        public Callback(Tab tab) {
            mTab = tab;
        }

        @Override
        public boolean onBackToSafety() {
            mSignaled = true;
            if (mSafetyPage == null) {
                return false;
            }

            mTab.getNavigationController().navigate(Uri.parse(mSafetyPage));
            return true;
        }
    }

    @Before
    public void setUp() throws Throwable {
        mActivity = mActivityTestRule.launchShellWithUrl(null);
        Assert.assertNotNull(mActivity);

        mGoodServer = TestWebServer.start();
        mGoodUrl = mGoodServer.setResponse("/ok.html", "<html>ok</html>", null);

        mBadSslServer = EmbeddedTestServer.createAndStartHTTPSServer(
                mActivity, ServerCertificate.CERT_MISMATCHED_NAME);
        mBadUrl = mBadSslServer.getURL("/weblayer/test/data/simple_page.html");

        mCallback = new Callback(mActivity.getTab());

        mActivityTestRule.navigateAndWait(mGoodUrl);
        mActivityTestRule.navigateAndWaitForFailure(mBadUrl);
    }

    @After
    public void tearDown() {
        mBadSslServer.stopAndDestroyServer();
    }

    /**
     * Verifies that if there's no ErrorPageCallback, when the user clicks "back to safety",
     * WebLayer provides default behavior (navigating back).
     */
    @Test
    @SmallTest
    public void testBackToSafetyDefaultBehavior() throws Throwable {
        NavigationWaiter navigationWaiter = new NavigationWaiter(
                mGoodUrl, mActivity.getTab(), false /* expectFailure */, true /* waitForPaint */);
        mActivityTestRule.executeScriptSync(
                "window.certificateErrorPageController.dontProceed();", false);
        navigationWaiter.waitForNavigation();
        Assert.assertFalse(mCallback.mSignaled);
    }

    /**
     * Verifies that if there's an ErrorPageCallback and onBackToSafety returns true, WebLayer does
     * *not* provide default behavior.
     */
    @Test
    @SmallTest
    public void testBackToSafetyOverride() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setErrorPageCallback(mCallback); });

        mCallback.mSafetyPage = mGoodServer.setResponse("/safe.html", "<html>safe</html>", null);

        NavigationWaiter navigationWaiter = new NavigationWaiter(mCallback.mSafetyPage,
                mActivity.getTab(), false /* expectFailure */, true /* waitForPaint */);
        mActivityTestRule.executeScriptSync(
                "window.certificateErrorPageController.dontProceed();", false);
        navigationWaiter.waitForNavigation();
        Assert.assertTrue(mCallback.mSignaled);
    }

    /**
     * Verifies that if there's an ErrorPageCallback and onBackToSafety returns false, WebLayer
     * *does* provide default behavior.
     */
    @Test
    @SmallTest
    public void testBackToSafetyDontOverride() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setErrorPageCallback(mCallback); });

        NavigationWaiter navigationWaiter = new NavigationWaiter(
                mGoodUrl, mActivity.getTab(), false /* expectFailure */, true /* waitForPaint */);
        mActivityTestRule.executeScriptSync(
                "window.certificateErrorPageController.dontProceed();", false);
        navigationWaiter.waitForNavigation();
        Assert.assertTrue(mCallback.mSignaled);
    }
}

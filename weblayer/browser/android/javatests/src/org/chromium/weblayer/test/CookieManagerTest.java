// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.CookieChangeCause;
import org.chromium.weblayer.CookieChangedCallback;
import org.chromium.weblayer.CookieManager;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.concurrent.TimeoutException;

/**
 * Tests that CookieManager works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class CookieManagerTest {
    private CookieManager mCookieManager;
    private Uri mBaseUri;

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Before
    public void setUp() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        mCookieManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return activity.getBrowser().getProfile().getCookieManager(); });
        mBaseUri = Uri.parse(mActivityTestRule.getTestServer().getURL("/"));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testSetCookie() throws Exception {
        Assert.assertTrue(setCookie("foo=bar"));

        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/echoheader?Cookie"));
        Assert.assertEquals("foo=bar",
                mActivityTestRule.executeScriptAndExtractString("document.body.textContent"));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testSetCookieInvalid() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                mCookieManager.setCookie(mBaseUri, "", null);
                Assert.fail("Exception not thrown.");
            } catch (IllegalArgumentException e) {
                Assert.assertEquals(e.getMessage(), "Invalid cookie: ");
            }
        });
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testSetCookieNotSet() throws Exception {
        Assert.assertFalse(setCookie("foo=bar; Secure"));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testSetCookieNullCallback() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCookieManager.setCookie(mBaseUri, "foo=bar", null); });

        // Do a navigation to make sure the cookie gets set.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestServer().getURL("/echo"));

        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/echoheader?Cookie"));
        Assert.assertEquals("foo=bar",
                mActivityTestRule.executeScriptAndExtractString("document.body.textContent"));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testGetCookie() throws Exception {
        Assert.assertEquals(getCookie(), "");
        Assert.assertTrue(setCookie("foo="));
        Assert.assertEquals(getCookie(), "foo=");
        Assert.assertTrue(setCookie("foo=bar"));
        Assert.assertEquals(getCookie(), "foo=bar");
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testCookieChanged() throws Exception {
        CookieChangedCallbackHelper helper = new CookieChangedCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCookieManager.addCookieChangedCallback(mBaseUri, null, helper); });
        Assert.assertTrue(setCookie("foo=bar"));
        helper.waitForChange();
        Assert.assertEquals(helper.getCause(), CookieChangeCause.INSERTED);
        Assert.assertEquals(helper.getCookie(), "foo=bar");

        Assert.assertTrue(setCookie("foo=baz"));
        helper.waitForChange();
        Assert.assertEquals(helper.getCause(), CookieChangeCause.OVERWRITE);
        Assert.assertEquals(helper.getCookie(), "foo=bar");
        helper.waitForChange();
        Assert.assertEquals(helper.getCause(), CookieChangeCause.INSERTED);
        Assert.assertEquals(helper.getCookie(), "foo=baz");
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testCookieChangedRemoveCallback() throws Exception {
        CookieChangedCallbackHelper helper = new CookieChangedCallbackHelper();
        Runnable remove = TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCookieManager.addCookieChangedCallback(mBaseUri, "cookie2", helper);
            return mCookieManager.addCookieChangedCallback(mBaseUri, "cookie1", helper);
        });
        Assert.assertTrue(setCookie("cookie1=something"));
        helper.waitForChange();
        Assert.assertEquals(helper.getCookie(), "cookie1=something");

        TestThreadUtils.runOnUiThreadBlocking(remove);

        // Set cookie1 first and then cookie2. We should only receive a cookie change event for
        // cookie2.
        Assert.assertTrue(setCookie("cookie1=other"));
        Assert.assertTrue(setCookie("cookie2=something"));
        helper.waitForChange();
        Assert.assertEquals(helper.getCookie(), "cookie2=something");
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(83)
    public void testCookieChangedRemoveCallbackAfterProfileDestroyed() throws Exception {
        // Removing change callback should be a no-op after the profile is destroyed.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = mActivityTestRule.getActivity().getBrowser().getProfile();
            Runnable remove = mCookieManager.addCookieChangedCallback(
                    mBaseUri, null, new CookieChangedCallbackHelper());
            // We need to remove the fragment before calling Profile#destroy().
            FragmentManager fm = mActivityTestRule.getActivity().getSupportFragmentManager();
            fm.beginTransaction().remove(fm.getFragments().get(0)).commitNow();

            profile.destroy();
            remove.run();
        });
    }

    private boolean setCookie(String value) throws Exception {
        return mActivityTestRule.setCookie(mCookieManager, mBaseUri, value);
    }

    private String getCookie() throws Exception {
        return mActivityTestRule.getCookie(mCookieManager, mBaseUri);
    }

    private static class CookieChangedCallbackHelper extends CookieChangedCallback {
        private CallbackHelper mCallbackHelper = new CallbackHelper();
        private int mCallCount;
        private int mCause;
        private String mCookie;

        public void waitForChange() throws TimeoutException {
            mCallbackHelper.waitForCallback(mCallCount);
            mCallCount++;
        }

        @CookieChangeCause
        public int getCause() {
            return mCause;
        }

        public String getCookie() {
            return mCookie;
        }

        @Override
        public void onCookieChanged(String cookie, @CookieChangeCause int cause) {
            mCookie = cookie;
            mCause = cause;
            mCallbackHelper.notifyCalled();
        }
    }
}

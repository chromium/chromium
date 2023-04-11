// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.core.content.ContextCompat;
import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.webengine.CookieManager;
import org.chromium.webengine.RestrictedAPIException;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.concurrent.CountDownLatch;

/**
 * Tests the CookieManager API.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class CookieManagerTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    @Rule
    public DigitalAssetLinksServerRule mDALServerRule = new DigitalAssetLinksServerRule();

    private TestWebServer mServer;
    private CookieManager mCookieManager;
    private TabManager mTabManager;

    private WebSandbox mSandbox;

    @Before
    public void setUp() throws Exception {
        mServer = mDALServerRule.getServer();
        mActivityTestRule.launchShell();

        mSandbox = mActivityTestRule.getWebSandbox();
        WebEngine webEngine = runOnUiThreadBlocking(() -> mSandbox.createWebEngine()).get();
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(webEngine.getFragment()));
        mCookieManager = webEngine.getCookieManager();
        mTabManager = webEngine.getTabManager();
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(() -> mSandbox.shutdown());
        mActivityTestRule.finish();
    }

    @Test
    @SmallTest
    public void accessFailsWithoutVerification() throws Exception {
        ListenableFuture<String> future =
                runOnUiThreadBlocking(() -> mCookieManager.getCookie(mServer.getBaseUrl()));

        CountDownLatch executeLatch = new CountDownLatch(1);
        Futures.addCallback(future, new FutureCallback<String>() {
            @Override
            public void onSuccess(String result) {
                Assert.fail("future resolved unexpectedly.");
            }

            @Override
            public void onFailure(Throwable thrown) {
                if (!(thrown instanceof RestrictedAPIException)) {
                    Assert.fail(
                            "expected future to fail due to RestrictedAPIException, instead got: "
                            + thrown.getClass().getName());
                }
                executeLatch.countDown();
            }
        }, ContextCompat.getMainExecutor(mActivityTestRule.getContext()));

        executeLatch.await();
    }

    @Test
    @SmallTest
    public void setAndGetCookies() throws Exception {
        mDALServerRule.setUpDigitalAssetLinks();
        String cookie =
                runOnUiThreadBlocking(() -> mCookieManager.getCookie(mServer.getBaseUrl())).get();
        Assert.assertEquals(cookie, "");
        runOnUiThreadBlocking(() -> mCookieManager.setCookie(mServer.getBaseUrl(), "foo=bar"));
        String setCookie =
                runOnUiThreadBlocking(() -> mCookieManager.getCookie(mServer.getBaseUrl())).get();
        Assert.assertEquals(setCookie, "foo=bar");
    }

    @Test
    @SmallTest
    public void getCookieCreatedByPage() throws Exception {
        mDALServerRule.setUpDigitalAssetLinks();

        String url = mDALServerRule.getServer().setResponse("/page.html",
                "<html><script>document.cookie='foo=bar42'</script><body>contents!</body></html>",
                null);

        String cookie =
                runOnUiThreadBlocking(() -> mCookieManager.getCookie(mServer.getBaseUrl())).get();
        Assert.assertEquals(cookie, "");
        Tab tab = mTabManager.getActiveTab();

        mActivityTestRule.navigateAndWait(tab, url);

        // Run JS to make sure the script had time to run before we actually check via the cookie
        // manager.
        Assert.assertEquals("\"foo=bar42\"",
                runOnUiThreadBlocking(() -> tab.executeScript("document.cookie", false)).get());

        String updatedCookie = runOnUiThreadBlocking(() -> mCookieManager.getCookie(url)).get();
        Assert.assertEquals(updatedCookie, "foo=bar42");
    }
}
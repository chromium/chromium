// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

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

import org.chromium.base.test.util.Batch;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.webengine.CookieManager;
import org.chromium.webengine.RestrictedAPIException;
import org.chromium.webengine.Tab;
import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebSandbox;

import java.util.concurrent.CountDownLatch;

/**
 * Tests the CookieManager API.
 */
@Batch(Batch.PER_CLASS)
@RunWith(WebEngineJUnit4ClassRunner.class)
public class CookieManagerTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    @Rule
    public DigitalAssetLinksServerRule mDALServerRule = new DigitalAssetLinksServerRule();

    private TestWebServer mServer;
    private CookieManager mCookieManager;

    @Before
    public void setUp() throws Exception {
        mServer = mDALServerRule.getServer();
        mActivityTestRule.launchShell();

        WebSandbox sandbox = mActivityTestRule.getWebSandbox();
        WebFragment fragment = runOnUiThreadBlocking(() -> sandbox.createFragment());
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(fragment));
        mCookieManager = fragment.getCookieManager().get();
    }

    @After
    public void tearDown() {
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
        }, mActivityTestRule.getContext().getMainExecutor());

        executeLatch.await();
    }

    @Test
    @SmallTest
    public void setAndGetCookies() throws Exception {
        mDALServerRule.setUpDigitalAssetLinks();
        Assert.assertEquals(mCookieManager.getCookie(mServer.getBaseUrl()).get(), "");
        mCookieManager.setCookie(mServer.getBaseUrl(), "foo=bar");
        Assert.assertEquals(mCookieManager.getCookie(mServer.getBaseUrl()).get(), "foo=bar");
    }

    @Test
    @SmallTest
    public void getCookieCreatedByPage() throws Exception {
        mDALServerRule.setUpDigitalAssetLinks();

        String url = mDALServerRule.getServer().setResponse("/page.html",
                "<html><script>document.cookie='foo=bar42'</script><body>contents!</body></html>",
                null);

        Assert.assertEquals(mCookieManager.getCookie(url).get(), "");
        Tab tab = mActivityTestRule.getFragment().getTabManager().get().getActiveTab().get();

        mActivityTestRule.navigateAndWait(tab, url);
        Assert.assertEquals(mCookieManager.getCookie(url).get(), "foo=bar42");
    }
}
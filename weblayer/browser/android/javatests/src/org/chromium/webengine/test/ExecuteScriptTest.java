// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.util.Pair;

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
import org.chromium.webengine.RestrictedAPIException;
import org.chromium.webengine.Tab;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * Tests executing JavaScript in a Tab.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class ExecuteScriptTest {
    private static final String sRequestPath = "/page.html";
    private static final String sResponseString =
            "<html><head></head><body>contents!</body><script>window.foo = 42;</script></html>";

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    @Rule
    public DigitalAssetLinksServerRule mDALServerRule = new DigitalAssetLinksServerRule();

    private Tab mTab;
    private String mDefaultUrl;
    private TestWebServer mServer;

    @Before
    public void setUp() throws Exception {
        mServer = mDALServerRule.getServer();
        mActivityTestRule.launchShell();

        List<Pair<String, String>> embedderAncestorHeader = new ArrayList();
        embedderAncestorHeader.add(new Pair("X-embedder-ancestors", "none"));
        mDefaultUrl = mServer.setResponse(sRequestPath, sResponseString, embedderAncestorHeader);
    }

    @After
    public void tearDown() {
        mActivityTestRule.finish();
    }

    private Tab navigate() throws Exception {
        mActivityTestRule.createWebEngineAttachThenNavigateAndWait(mDefaultUrl);
        return mActivityTestRule.getFragment().getWebEngine().getTabManager().getActiveTab();
    }

    @Test
    @SmallTest
    public void executeScriptFailsWithoutVerification() throws Exception {
        Tab activeTab = navigate();

        CountDownLatch executeLatch = new CountDownLatch(1);

        ListenableFuture<String> future =
                runOnUiThreadBlocking(() -> activeTab.executeScript("1+1", true));

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
    public void executeScriptSucceedsWithDALVerification() throws Exception {
        mDALServerRule.setUpDigitalAssetLinks();
        Tab activeTab = navigate();

        ListenableFuture<String> future =
                runOnUiThreadBlocking(() -> activeTab.executeScript("1+1", true));
        Assert.assertEquals(future.get(), "2");
    }

    @Test
    @SmallTest
    public void executeScriptSucceedsWithHeaderVerification() throws Exception {
        List<Pair<String, String>> embedderAncestorHeader = new ArrayList();
        embedderAncestorHeader.add(
                new Pair("X-embedder-ancestors", mActivityTestRule.getPackageName()));
        mDefaultUrl = mServer.setResponse(sRequestPath, sResponseString, embedderAncestorHeader);
        Tab activeTab = navigate();

        ListenableFuture<String> future =
                runOnUiThreadBlocking(() -> activeTab.executeScript("1+1", true));
        Assert.assertEquals(future.get(), "2");
    }

    @Test
    @SmallTest
    public void executeScriptSucceedsWithoutHeader() throws Exception {
        mDefaultUrl = mServer.setResponse(sRequestPath, sResponseString, null);
        Tab activeTab = navigate();

        ListenableFuture<String> future =
                runOnUiThreadBlocking(() -> activeTab.executeScript("1+1", true));
        Assert.assertEquals(future.get(), "2");
    }

    @Test
    @SmallTest
    public void useSeparateIsolate() throws Exception {
        mDALServerRule.setUpDigitalAssetLinks();
        Tab activeTab = navigate();

        ListenableFuture<String> futureIsolated =
                runOnUiThreadBlocking(() -> activeTab.executeScript("window.foo", true));
        Assert.assertEquals(futureIsolated.get(), "null");

        ListenableFuture<String> futureUnisolated =
                runOnUiThreadBlocking(() -> activeTab.executeScript("window.foo", false));
        Assert.assertEquals(futureUnisolated.get(), "42");
    }

    @Test
    @SmallTest
    public void modifyDOMSucceeds() throws Exception {
        mDALServerRule.setUpDigitalAssetLinks();
        Tab activeTab = navigate();

        ListenableFuture<String> future1 = runOnUiThreadBlocking(
                () -> activeTab.executeScript("document.body.innerText", false));
        Assert.assertEquals(future1.get(), "\"contents!\"");

        runOnUiThreadBlocking(
                () -> activeTab.executeScript("document.body.innerText = 'foo'", false))
                .get();

        ListenableFuture<String> future2 = runOnUiThreadBlocking(
                () -> activeTab.executeScript("document.body.innerText", false));
        Assert.assertEquals(future2.get(), "\"foo\"");
    }
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.webengine.RestrictedAPIException;
import org.chromium.webengine.Tab;

import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * Tests executing JavaScript in a Tab.
 */
@Batch(Batch.PER_CLASS)
@RunWith(WebEngineJUnit4ClassRunner.class)
public class ExecuteScriptTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final String ASSETLINKS_PATH = "/.well-known/assetlinks.json";
    // TODO(crbug.com/1376522): Figure out how to not hardcode a port number.
    private static final int PORT = 8888;

    private TestWebServer mServer;
    private Tab mTab;
    private String mDefaultUrl;

    @Before
    public void setUp() throws Exception {
        mServer = TestWebServer.start(PORT);
        mActivityTestRule.launchShell();

        mDefaultUrl = mServer.setResponse("/page.html",
                "<html><head></head><body>contents!</body><script>window.foo = 42;</script></html>",
                null);
        // By default, the asset links are not set up.
        mServer.setResponseWithNotFoundStatus(ASSETLINKS_PATH, null);
    }

    @After
    public void tearDown() {
        mServer.shutdown();
        mActivityTestRule.finish();
    }

    private Tab navigate() throws Exception {
        mActivityTestRule.attachNewFragmentThenNavigateAndWait(mDefaultUrl);
        return mActivityTestRule.getFragment().getTabManager().get().getActiveTab().get();
    }

    private static String makeAssetFile(String packageName, String fingerprint) {
        try {
            return (new JSONArray().put(
                            new JSONObject()
                                    .put("relation",
                                            new JSONArray().put(
                                                    "delegate_permission/common.handle_all_urls"))
                                    .put("target",
                                            new JSONObject()
                                                    .put("namespace", "android_app")
                                                    .put("package_name", packageName)
                                                    .put("sha256_cert_fingerprints",
                                                            new JSONArray().put(fingerprint)))))
                    .toString();
        } catch (JSONException e) {
        }
        return "";
    }

    private void setUpDigitalAssetLinks() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        List<String> signatureFingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(packageName);
        mServer.setResponse(
                ASSETLINKS_PATH, makeAssetFile(packageName, signatureFingerprints.get(0)), null);
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
        }, mActivityTestRule.getContext().getMainExecutor());

        executeLatch.await();
    }

    @Test
    @SmallTest
    public void executeScriptSucceedsWithVerification() throws Exception {
        setUpDigitalAssetLinks();
        Tab activeTab = navigate();

        ListenableFuture<String> future =
                runOnUiThreadBlocking(() -> activeTab.executeScript("1+1", true));
        Assert.assertEquals(future.get(), "2");
    }

    @Test
    @SmallTest
    public void useSeparateIsolate() throws Exception {
        setUpDigitalAssetLinks();
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
        setUpDigitalAssetLinks();
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

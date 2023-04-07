// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabObserver;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests that a tab's favicon is returned.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class FaviconTest {
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mServer;
    private Tab mTab;

    private String getTestDataURL(String path) {
        return mServer.getURL("/weblayer/test/data/" + path);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();
        mServer = mTestServerRule.getServer();
        WebSandbox sandbox = mActivityTestRule.getWebSandbox();
        WebEngine webEngine = runOnUiThreadBlocking(() -> sandbox.createWebEngine()).get();

        mTab = webEngine.getTabManager().getActiveTab();
    }

    @After
    public void tearDown() {
        mActivityTestRule.finish();
    }

    private static final class ResultHolder {
        public Bitmap mResult;
    }

    private Bitmap waitForFaviconChange() throws Exception {
        final ResultHolder holder = new ResultHolder();
        CountDownLatch faviconLatch = new CountDownLatch(1);
        TabObserver observer = new TabObserver() {
            @Override
            public void onFaviconChanged(Tab tab, Bitmap favicon) {
                holder.mResult = favicon;
                faviconLatch.countDown();
            }
        };
        runOnUiThreadBlocking(() -> mTab.registerTabObserver(observer));

        faviconLatch.await(10, TimeUnit.SECONDS);

        runOnUiThreadBlocking(() -> mTab.unregisterTabObserver(observer));
        return holder.mResult;
    }

    private void verifyFavicon(Bitmap favicon) {
        Assert.assertNotNull(favicon);
        Assert.assertEquals(favicon.getHeight(), 16);
        Assert.assertEquals(favicon.getWidth(), 16);
        Assert.assertEquals(favicon.getPixel(0, 0), Color.rgb(0, 72, 255));
    }

    @Test
    @SmallTest
    public void checkFaviconIsExposed() throws Exception {
        runOnUiThreadBlocking(()
                                      -> mTab.getNavigationController().navigate(
                                              getTestDataURL("simple_page_with_favicon.html")));
        Bitmap favicon = waitForFaviconChange();
        verifyFavicon(favicon);
    }

    @Test
    @SmallTest
    public void checkDelayedFaviconsAreDelivered() throws Exception {
        runOnUiThreadBlocking(()
                                      -> mTab.getNavigationController().navigate(getTestDataURL(
                                              "simple_page_with_delayed_favicon.html")));

        Bitmap favicon = waitForFaviconChange();
        Assert.assertNull(favicon);

        // The page dynamically creates a favicon after receiving a post message.
        runOnUiThreadBlocking(() -> mTab.postMessage("message", "*"));
        favicon = waitForFaviconChange();
        verifyFavicon(favicon);
    }

    @Test
    @MediumTest
    public void checkFaviconsCanBeDeleted() throws Exception {
        runOnUiThreadBlocking(()
                                      -> mTab.getNavigationController().navigate(getTestDataURL(
                                              "simple_page_with_deleted_favicon.html")));

        Bitmap favicon = waitForFaviconChange();
        verifyFavicon(favicon);

        // Favicon eventually gets deleted.
        favicon = waitForFaviconChange();
        Assert.assertNull(favicon);
    }

    @Test
    @SmallTest
    public void pageWithNoFaviconDeliversEvent() throws Exception {
        runOnUiThreadBlocking(
                () -> mTab.getNavigationController().navigate(getTestDataURL("simple_page.html")));

        Bitmap favicon = waitForFaviconChange();
        Assert.assertNull(favicon);
    }

    @Test
    @SmallTest
    public void multipleNavigationsDelieverEvents() throws Exception {
        runOnUiThreadBlocking(()
                                      -> mTab.getNavigationController().navigate(
                                              getTestDataURL("simple_page_with_favicon.html")));

        Bitmap favicon = waitForFaviconChange();
        verifyFavicon(favicon);

        runOnUiThreadBlocking(
                () -> mTab.getNavigationController().navigate(getTestDataURL("simple_page.html")));
        favicon = waitForFaviconChange();
        Assert.assertNull(favicon);
    }
}

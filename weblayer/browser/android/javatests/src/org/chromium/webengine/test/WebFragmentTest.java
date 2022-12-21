// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.Tab;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

/**
 * Tests that basic fragment operations work as intended.
 */
@Batch(Batch.PER_CLASS)
@RunWith(WebEngineJUnit4ClassRunner.class)
public class WebFragmentTest {
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mServer;
    private WebSandbox mWebSandbox;

    @Before
    public void setUp() throws Throwable {
        mServer = mTestServerRule.getServer();
        mActivityTestRule.launchShell();
        mWebSandbox = mActivityTestRule.getWebSandbox();
    }

    @After
    public void tearDown() {
        mActivityTestRule.finish();
        runOnUiThreadBlocking(() -> mWebSandbox.shutdown());
    }

    private String getTestDataURL(String path) {
        return mServer.getURL("/weblayer/test/data/" + path);
    }

    @Test
    @SmallTest
    public void loadsPage() throws Exception {
        WebEngine webEngine = runOnUiThreadBlocking(() -> mWebSandbox.createWebEngine()).get();
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(webEngine.getFragment()));

        Tab activeTab = webEngine.getTabManager().getActiveTab();

        Assert.assertEquals(activeTab.getDisplayUri(), Uri.EMPTY);

        String url = getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(activeTab, url);

        Assert.assertEquals(runOnUiThreadBlocking(() -> activeTab.getDisplayUri()), Uri.parse(url));
    }

    /**
     * This test is similar to the previous one and just ensures that these unit tests can be
     * batched.
     */
    @Test
    @SmallTest
    public void successfullyLoadDifferentPage() throws Exception {
        mActivityTestRule.createWebEngineAttachThenNavigateAndWait(
                getTestDataURL("simple_page2.html"));
    }

    @Test
    @SmallTest
    public void fragmentTabCanLoadMultiplePages() throws Exception {
        mActivityTestRule.createWebEngineAttachThenNavigateAndWait(
                getTestDataURL("simple_page.html"));

        Tab tab = mActivityTestRule.getActiveTab();
        mActivityTestRule.navigateAndWait(tab, getTestDataURL("simple_page2.html"));

        Assert.assertTrue(tab.getDisplayUri().toString().endsWith("simple_page2.html"));
    }

    @Test
    @SmallTest
    public void fragmentsCanBeReplaced() throws Exception {
        mActivityTestRule.createWebEngineAttachThenNavigateAndWait(
                getTestDataURL("simple_page.html"));
        // New fragment
        mActivityTestRule.createWebEngineAttachThenNavigateAndWait(
                getTestDataURL("simple_page2.html"));

        Tab tab = mActivityTestRule.getActiveTab();
        Assert.assertTrue(tab.getDisplayUri().toString().endsWith("simple_page2.html"));
    }

    @Test
    @SmallTest
    public void navigationFailure() {
        try {
            mActivityTestRule.createWebEngineAttachThenNavigateAndWait(
                    getTestDataURL("missingpage.html"));
            Assert.fail("exception not thrown");
        } catch (RuntimeException e) {
            Assert.assertEquals(e.getMessage(), "Navigation failed.");
        } catch (Exception e) {
            Assert.fail("RuntimeException not thrown, instead got: " + e);
        }
    }
}

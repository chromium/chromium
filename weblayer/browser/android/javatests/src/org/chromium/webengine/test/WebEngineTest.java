// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.annotation.NonNull;
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
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;

/**
 * Tests functions called on the WebEngine object.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class WebEngineTest {
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mServer;

    WebSandbox mSandbox;
    WebEngine mWebEngine;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();
        mServer = mTestServerRule.getServer();

        mSandbox = mActivityTestRule.getWebSandbox();
        mWebEngine = runOnUiThreadBlocking(() -> mSandbox.createWebEngine()).get();
    }

    @After
    public void shutdown() throws Exception {
        if (mSandbox != null) {
            runOnUiThreadBlocking(() -> mSandbox.shutdown());
        }
        mActivityTestRule.finish();
    }

    private String getTestDataURL(String path) {
        return mServer.getURL("/weblayer/test/data/" + path);
    }

    @Test
    @SmallTest
    public void tabManagerIsAvailable() throws Exception {
        Assert.assertNotNull(mWebEngine.getTabManager());
    }

    @Test
    @SmallTest
    public void tabCookieIsAvailable() throws Exception {
        Assert.assertNotNull(mWebEngine.getCookieManager());
    }

    @Test
    @SmallTest
    public void navigateBackEngineOnInitialStateReturnsFalse() throws Exception {
        boolean handledBackNav = runOnUiThreadBlocking(() -> mWebEngine.tryNavigateBack()).get();
        Assert.assertFalse(handledBackNav);
    }

    @Test
    @SmallTest
    public void navigateBackEngineHandlesBackNavInTab() throws Exception {
        String url1 = getTestDataURL("simple_page.html");
        String url2 = getTestDataURL("simple_page2.html");
        Tab activeTab = mWebEngine.getTabManager().getActiveTab();
        mActivityTestRule.navigateAndWait(activeTab, url1);
        mActivityTestRule.navigateAndWait(activeTab, url2);

        // Go back and wait until navigation completed.
        CountDownLatch navigationCompletedLatch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            activeTab.getNavigationController().registerNavigationObserver(
                    new NavigationObserver() {
                        @Override
                        public void onNavigationCompleted(@NonNull Navigation navigation) {
                            navigationCompletedLatch.countDown();
                        }
                    });
        });
        boolean handledBackNav = runOnUiThreadBlocking(() -> mWebEngine.tryNavigateBack()).get();
        navigationCompletedLatch.await();

        Assert.assertTrue(handledBackNav);
        Assert.assertEquals(1, mWebEngine.getTabManager().getAllTabs().size());
        Assert.assertEquals(
                url1, mWebEngine.getTabManager().getActiveTab().getDisplayUri().toString());
    }

    @Test
    @SmallTest
    public void navigateBackEngineClosesTabAndSetsPreviousTabActive() throws Exception {
        Tab firstTab = mWebEngine.getTabManager().getActiveTab();
        String url1 = getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(firstTab, url1);

        Tab secondTab = runOnUiThreadBlocking(() -> mWebEngine.getTabManager().createTab()).get();
        Assert.assertEquals(2, mWebEngine.getTabManager().getAllTabs().size());

        // Synchronously Set second Tab to active.
        CountDownLatch secondTabActiveLatch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            mWebEngine.getTabManager().registerTabListObserver(new TabListObserver() {
                @Override
                public void onActiveTabChanged(@NonNull Tab activeTab) {
                    secondTabActiveLatch.countDown();
                }
            });
            secondTab.setActive();
        });
        secondTabActiveLatch.await();
        Assert.assertEquals(secondTab, mWebEngine.getTabManager().getActiveTab());

        String url2 = getTestDataURL("simple_page2.html");
        mActivityTestRule.navigateAndWait(secondTab, url2);

        // // Go back and wait until active tab changed and second tab was removed.
        CountDownLatch activeTabChangedLatch = new CountDownLatch(1);
        CountDownLatch prevTabRemovedLatch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            mWebEngine.getTabManager().registerTabListObserver(new TabListObserver() {
                @Override
                public void onActiveTabChanged(@NonNull Tab activeTab) {
                    activeTabChangedLatch.countDown();
                }
                @Override
                public void onTabRemoved(@NonNull Tab activeTab) {
                    prevTabRemovedLatch.countDown();
                }
            });
        });
        boolean handledBackNav = runOnUiThreadBlocking(() -> mWebEngine.tryNavigateBack()).get();
        activeTabChangedLatch.await();
        prevTabRemovedLatch.await();

        Assert.assertTrue(handledBackNav);
        Assert.assertEquals(1, mWebEngine.getTabManager().getAllTabs().size());
        Assert.assertEquals(firstTab, mWebEngine.getTabManager().getActiveTab());
        Assert.assertEquals(
                url1, mWebEngine.getTabManager().getActiveTab().getDisplayUri().toString());
    }

    @Test
    @SmallTest
    public void closingWebEngineRemovesItFromSandbox() throws Exception {
        int numWebEngines = runOnUiThreadBlocking(() -> {
            mWebEngine.close();
            return mSandbox.getWebEngines().size();
        });
        Assert.assertEquals(0, numWebEngines);
    }

    @Test
    @SmallTest
    public void createWebEngineWithTag() throws Exception {
        String tag = "web-engine-tag";
        WebEngine webEngineWithTag =
                runOnUiThreadBlocking(() -> mSandbox.createWebEngine(tag)).get();

        Assert.assertEquals(webEngineWithTag, mSandbox.getWebEngine(tag));
        Assert.assertEquals(tag, webEngineWithTag.getTag());
    }

    @Test
    @SmallTest
    public void createWebEngineWithoutTag() throws Exception {
        WebEngine webEngineWithoutGivenTag =
                runOnUiThreadBlocking(() -> mSandbox.createWebEngine()).get();

        Assert.assertTrue(mSandbox.getWebEngines().contains(webEngineWithoutGivenTag));
        // Check that a tag was given to the web-engine
        Assert.assertEquals("webengine_1", webEngineWithoutGivenTag.getTag());
    }

    @Test
    @SmallTest
    public void cannotCreateWebEngineWithIdenticalTags() throws Exception {
        String tag = "web-engine-tag";
        WebEngine webEngineWithTag =
                runOnUiThreadBlocking(() -> mSandbox.createWebEngine(tag)).get();

        Assert.assertThrows(ExecutionException.class,
                () -> runOnUiThreadBlocking(() -> mSandbox.createWebEngine(tag)));
    }
}
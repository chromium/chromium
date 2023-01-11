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
import org.chromium.webengine.TabNavigationController;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.concurrent.CountDownLatch;

/**
 * Tests various aspects of Navigations and NavigationObservers.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class TabNavigationControllerTest {
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mServer;

    private WebSandbox mSandbox;

    private TabNavigationController mNavigationController;
    private Tab mActiveTab;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();
        mServer = mTestServerRule.getServer();

        mSandbox = mActivityTestRule.getWebSandbox();
        WebEngine webEngine = runOnUiThreadBlocking(() -> mSandbox.createWebEngine()).get();

        mActiveTab = webEngine.getTabManager().getActiveTab();
        mNavigationController = mActiveTab.getNavigationController();
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

    private void navigateSync(String url) throws InterruptedException {
        CountDownLatch navigationCompleted = new CountDownLatch(1);

        runOnUiThreadBlocking(() -> {
            mNavigationController.registerNavigationObserver(new NavigationObserver() {
                @Override
                public void onNavigationCompleted(@NonNull Navigation navigation) {
                    navigationCompleted.countDown();
                }
            });
            mNavigationController.navigate(url);
        });
        navigationCompleted.await();
    }

    @Test
    @SmallTest
    public void cannotGoBackOnFirstLoadedUrl() throws Exception {
        Assert.assertFalse((runOnUiThreadBlocking(() -> mNavigationController.canGoBack())).get());
        navigateSync(getTestDataURL("simple_page.html"));
        Assert.assertFalse((runOnUiThreadBlocking(() -> mNavigationController.canGoBack())).get());
    }

    @Test
    @SmallTest
    public void canGoBackOnSecondLoadedUrl() throws Exception {
        navigateSync(getTestDataURL("simple_page.html"));
        navigateSync(getTestDataURL("simple_page2.html"));
        Assert.assertTrue((runOnUiThreadBlocking(() -> mNavigationController.canGoBack())).get());
    }

    @Test
    @SmallTest
    public void cannotGoForwardWhenNotNavigatedBackBefore() throws Exception {
        navigateSync(getTestDataURL("simple_page.html"));
        Assert.assertFalse(
                (runOnUiThreadBlocking(() -> mNavigationController.canGoForward())).get());
    }

    @Test
    @SmallTest
    public void backNavigationToFirstPage() throws Exception {
        navigateSync(getTestDataURL("simple_page.html"));
        navigateSync(getTestDataURL("simple_page2.html"));

        // Synchronously go back.
        CountDownLatch navigationCompleted = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            mNavigationController.registerNavigationObserver(new NavigationObserver() {
                @Override
                public void onNavigationCompleted(@NonNull Navigation navigation) {
                    navigationCompleted.countDown();
                }
            });
            mNavigationController.goBack();
        });
        navigationCompleted.await();

        Assert.assertEquals(
                getTestDataURL("simple_page.html"), mActiveTab.getDisplayUri().toString());
        Assert.assertFalse((runOnUiThreadBlocking(() -> mNavigationController.canGoBack())).get());
        Assert.assertTrue(
                (runOnUiThreadBlocking(() -> mNavigationController.canGoForward())).get());
    }

    @Test
    @SmallTest
    public void forwardNavigationAfterBackNavigation() throws Exception {
        navigateSync(getTestDataURL("simple_page.html"));
        navigateSync(getTestDataURL("simple_page2.html"));

        // Synchronously go back.
        CountDownLatch backNavigationCompleted = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            mNavigationController.registerNavigationObserver(new NavigationObserver() {
                @Override
                public void onNavigationCompleted(@NonNull Navigation navigation) {
                    backNavigationCompleted.countDown();
                }
            });
            mNavigationController.goBack();
        });
        backNavigationCompleted.await();
        Assert.assertEquals(
                getTestDataURL("simple_page.html"), mActiveTab.getDisplayUri().toString());

        // Synchronously go forward
        CountDownLatch forwardNavigationCompleted = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            mNavigationController.registerNavigationObserver(new NavigationObserver() {
                @Override
                public void onNavigationCompleted(@NonNull Navigation navigation) {
                    forwardNavigationCompleted.countDown();
                }
            });
            mNavigationController.goForward();
        });
        forwardNavigationCompleted.await();
        Assert.assertEquals(
                getTestDataURL("simple_page2.html"), mActiveTab.getDisplayUri().toString());
    }

    @Test
    @SmallTest
    public void testAllNavigationEventsAreReceivedOnNavigate() throws Exception {
        CountDownLatch navigationCompleted = new CountDownLatch(1);
        CountDownLatch navigationStarted = new CountDownLatch(1);
        CountDownLatch finishedLoadProgress = new CountDownLatch(1);

        runOnUiThreadBlocking(() -> {
            mNavigationController.registerNavigationObserver(new NavigationObserver() {
                @Override
                public void onNavigationStarted(@NonNull Navigation navigation) {
                    navigationStarted.countDown();
                }
                @Override
                public void onNavigationCompleted(@NonNull Navigation navigation) {
                    navigationCompleted.countDown();
                }
                @Override
                public void onLoadProgressChanged(double progress) {
                    if (progress == 1.0) {
                        finishedLoadProgress.countDown();
                    }
                }
            });

            mNavigationController.navigate(getTestDataURL("simple_page.html"));
        });
        navigationStarted.await();
        navigationCompleted.await();
        finishedLoadProgress.await();
    }
}
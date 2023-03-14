// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.FullscreenCallback;
import org.chromium.webengine.Tab;
import org.chromium.webengine.WebEngine;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests Fullscreen callbacks.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class FullscreenCallbackTest {
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mServer;
    private WebEngine mWebEngine;
    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();
        mServer = mTestServerRule.getServer();

        mWebEngine = mActivityTestRule.createWebEngineAttachThenNavigateAndWait(
                getTestDataURL("fullscreen.html"));
        mTab = mWebEngine.getTabManager().getActiveTab();
    }

    @After
    public void shutdown() throws Exception {
        mActivityTestRule.finish();
    }

    private String getTestDataURL(String path) {
        return mServer.getURL("/weblayer/test/data/" + path);
    }

    @Test
    @SmallTest
    public void fullscreenCallbacksAreCalled() throws Exception {
        CountDownLatch enterFullscreenLatch = new CountDownLatch(1);
        CountDownLatch exitFullscreenLatch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            mTab.setFullscreenCallback(new FullscreenCallback() {
                @Override
                public void onEnterFullscreen(WebEngine webEngine, Tab tab) {
                    Assert.assertEquals(mWebEngine, webEngine);
                    Assert.assertEquals(mTab, tab);
                    enterFullscreenLatch.countDown();
                }

                @Override
                public void onExitFullscreen(WebEngine webEngine, Tab tab) {
                    Assert.assertEquals(mWebEngine, webEngine);
                    Assert.assertEquals(mTab, tab);
                    exitFullscreenLatch.countDown();
                }
            });
        });
        // requestFullscreen()-js function expects to be invoked by an actual click.
        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getFragmentContainerView(), 1, 1);
        enterFullscreenLatch.await();
        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getFragmentContainerView(), 1, 1);
        exitFullscreenLatch.await();
    }

    @Test
    @SmallTest
    public void fullscreenIsEndedWhenTabIsInactive() throws Exception {
        CountDownLatch enterFullscreenLatch = new CountDownLatch(1);
        CountDownLatch exitFullscreenLatch = new CountDownLatch(1);
        CountDownLatch reenterFullscreenLatch = new CountDownLatch(2);
        runOnUiThreadBlocking(() -> {
            mTab.setFullscreenCallback(new FullscreenCallback() {
                @Override
                public void onEnterFullscreen(WebEngine webEngine, Tab tab) {
                    Assert.assertEquals(mWebEngine, webEngine);
                    Assert.assertEquals(mTab, tab);
                    enterFullscreenLatch.countDown();
                    reenterFullscreenLatch.countDown();
                }

                @Override
                public void onExitFullscreen(WebEngine webEngine, Tab tab) {
                    Assert.assertEquals(mWebEngine, webEngine);
                    Assert.assertEquals(mTab, tab);
                    exitFullscreenLatch.countDown();
                }
            });
        });
        // requestFullscreen()-js function expects to be invoked by an actual click.
        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getFragmentContainerView(), 1, 1);
        enterFullscreenLatch.await();

        Tab secondTab = runOnUiThreadBlocking(() -> mWebEngine.getTabManager().createTab()).get();
        mActivityTestRule.setTabActiveAndWait(mWebEngine, secondTab);
        exitFullscreenLatch.await();
        mActivityTestRule.setTabActiveAndWait(mWebEngine, mTab);
        Assert.assertFalse(reenterFullscreenLatch.await(3L, TimeUnit.SECONDS));
    }
}

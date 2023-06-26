// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.MessageEventListener;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabObserver;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests the postMessage functionality.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class PostMessageTest {
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

        // First title is the domain, second title is the one in the html title tag.
        CountDownLatch titleUpdateLatch = new CountDownLatch(2);
        TabObserver observer = new TabObserver() {
            @Override
            public void onTitleUpdated(Tab tab, String title) {
                titleUpdateLatch.countDown();
            }
        };
        runOnUiThreadBlocking(() -> mTab.registerTabObserver(observer));

        mActivityTestRule.navigateAndWait(mTab, getTestDataURL("postmessage.html"));
        titleUpdateLatch.await(10, TimeUnit.SECONDS);

        runOnUiThreadBlocking(() -> mTab.unregisterTabObserver(observer));
    }

    private static final class ResultHolder {
        public String mResult;
    }

    private String waitForTitleChange() throws Exception {
        final ResultHolder holder = new ResultHolder();
        CountDownLatch postMessageLatch = new CountDownLatch(1);
        TabObserver observer = new TabObserver() {
            @Override
            public void onTitleUpdated(Tab tab, String title) {
                holder.mResult = title;
                postMessageLatch.countDown();
            }
        };
        runOnUiThreadBlocking(() -> mTab.registerTabObserver(observer));

        postMessageLatch.await(10, TimeUnit.SECONDS);

        runOnUiThreadBlocking(() -> mTab.unregisterTabObserver(observer));

        if (holder.mResult == null) {
            throw new RuntimeException("Title was not updated");
        }
        return holder.mResult;
    }

    private String waitForPostMessage() throws Exception {
        return waitForPostMessage(Arrays.asList("*"));
    }

    private String waitForPostMessage(List<String> allowedOrigins) throws Exception {
        final ResultHolder holder = new ResultHolder();
        CountDownLatch postMessageLatch = new CountDownLatch(1);
        MessageEventListener listener = new MessageEventListener() {
            @Override
            public void onMessage(Tab source, String message) {
                Assert.assertEquals(source, mTab);
                holder.mResult = message;
                postMessageLatch.countDown();
            }
        };

        runOnUiThreadBlocking(() -> mTab.addMessageEventListener(listener, allowedOrigins));

        postMessageLatch.await(10, TimeUnit.SECONDS);
        runOnUiThreadBlocking(() -> mTab.removeMessageEventListener(listener));

        if (holder.mResult == null) {
            throw new RuntimeException("postMessage was not received");
        }
        return holder.mResult;
    }

    @Test
    @SmallTest
    public void pageReceivesPostMessage() throws Exception {
        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        Assert.assertEquals("postMessage: 1", waitForTitleChange());
    }

    @Test
    @SmallTest
    public void pageCanPostMessageBack() throws Exception {
        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        Assert.assertEquals("message: hello, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1457935")
    public void postMessageTargetOriginIsRespected() throws Exception {
        runOnUiThreadBlocking(() -> mTab.postMessage("hello", mTestServerRule.getOrigin()));
        Assert.assertEquals("postMessage: 1", waitForTitleChange());

        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "https://example.com/"));
        try {
            waitForPostMessage();
            Assert.fail("postMessage was delivered to the wrong origin");
        } catch (Exception e) {
        }
    }

    @Test
    @SmallTest
    public void canPostToPageMultipleTimes() throws Exception {
        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        Assert.assertEquals("message: hello, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());
        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        Assert.assertEquals("message: hello, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());
        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        Assert.assertEquals("postMessage: 3", waitForTitleChange());
    }

    @Test
    @MediumTest
    public void onlyTargetedTabReceivesPostMessage() throws Exception {
        WebSandbox sandbox = mActivityTestRule.getWebSandbox();
        WebEngine webEngine = runOnUiThreadBlocking(() -> sandbox.createWebEngine()).get();

        Tab tab2 = runOnUiThreadBlocking(() -> webEngine.getTabManager().createTab()).get();
        mActivityTestRule.navigateAndWait(tab2, getTestDataURL("postmessage.html"));

        runOnUiThreadBlocking(() -> tab2.postMessage("hello", "*"));
        try {
            waitForPostMessage();
            Assert.fail("postMessage was delivered to the wrong origin");
        } catch (Exception e) {
        }
    }

    @Test
    @MediumTest
    public void postMessageFromTabRespectsAllowedOrigin() throws Exception {
        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        Assert.assertEquals("message: hello, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());

        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        try {
            waitForPostMessage(Arrays.asList("https://different.example.com"));
            Assert.fail("postMessage was received from the wrong origin");
        } catch (Exception e) {
        }
    }

    @Test
    @MediumTest
    public void receivePostMessageFromSavedPort() throws Exception {
        runOnUiThreadBlocking(() -> mTab.postMessage("hello - delayed", "*"));
        Assert.assertEquals("message: hello - delayed, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());
        Assert.assertEquals("message: hello - delayed2, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());
        Assert.assertEquals("message: hello - delayed3, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());

        runOnUiThreadBlocking(() -> mTab.postMessage("hello", "*"));
        Assert.assertEquals("message: hello, "
                        + "source: app://org.chromium.webengine.test.instrumentation_test_apk",
                waitForPostMessage());
    }
}

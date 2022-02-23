// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.WebMessage;
import org.chromium.weblayer.WebMessageCallback;
import org.chromium.weblayer.WebMessageReplyProxy;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Verifies WebMessage related APIs.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class WebMessageTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // WebMessageCallback that stores last values supplied to onWebMessageReceived().
    private static final class WebMessageCallbackImpl extends WebMessageCallback {
        public WebMessageReplyProxy mLastProxy;
        public WebMessage mLastMessage;
        public CallbackHelper mCallbackHelper;
        public CallbackHelper mClosedCallbackHelper;
        public CallbackHelper mActiveChangedCallbackHelper;
        public WebMessageReplyProxy mProxyClosed;

        WebMessageCallbackImpl(CallbackHelper callbackHelper) {
            mCallbackHelper = callbackHelper;
        }
        @Override
        public void onWebMessageReceived(WebMessageReplyProxy replyProxy, WebMessage message) {
            mLastProxy = replyProxy;
            mLastMessage = message;
            mCallbackHelper.notifyCalled();
        }

        public void reset() {
            mLastProxy = null;
            mLastMessage = null;
            mProxyClosed = null;
        }

        @Override
        public void onWebMessageReplyProxyClosed(WebMessageReplyProxy replyProxy) {
            mProxyClosed = replyProxy;
            if (mClosedCallbackHelper != null) mClosedCallbackHelper.notifyCalled();
        }

        @Override
        public void onWebMessageReplyProxyActiveStateChanged(WebMessageReplyProxy replyProxy) {
            if (mActiveChangedCallbackHelper != null) mActiveChangedCallbackHelper.notifyCalled();
        }
    }

    @Test
    @SmallTest
    public void testPostMessage() throws Exception {
        // Load a page with a form.
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        assertNotNull(activity);
        CallbackHelper callbackHelper = new CallbackHelper();
        WebMessageCallbackImpl webMessageCallback = new WebMessageCallbackImpl(callbackHelper);
        runOnUiThreadBlocking(() -> {
            activity.getTab().registerWebMessageCallback(
                    webMessageCallback, "x", Arrays.asList("*"));
        });

        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestDataURL("web_message_test.html"));
        // web_message_test.html posts a message, wait for it.
        callbackHelper.waitForCallback(0);
        assertNotNull(webMessageCallback.mLastMessage);
        assertNotNull(webMessageCallback.mLastProxy);
        assertEquals("from page", webMessageCallback.mLastMessage.getContents());
        final WebMessageReplyProxy lastProxy = webMessageCallback.mLastProxy;
        int majorVersion = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> WebLayer.getSupportedMajorVersion(mActivityTestRule.getActivity()));
        if (majorVersion >= 99) {
            assertNotNull(runOnUiThreadBlocking(() -> { return lastProxy.getPage(); }));
        }
        webMessageCallback.reset();

        int currentCallCount = callbackHelper.getCallCount();
        // Send a message, which the page should ack back.
        runOnUiThreadBlocking(() -> { lastProxy.postMessage(new WebMessage("Z")); });
        callbackHelper.waitForCallback(currentCallCount);
        assertNotNull(webMessageCallback.mLastMessage);
        assertEquals("bouncing Z", webMessageCallback.mLastMessage.getContents());
        assertEquals(lastProxy, webMessageCallback.mLastProxy);

        runOnUiThreadBlocking(() -> { activity.getTab().unregisterWebMessageCallback("x"); });
    }

    @Test
    @SmallTest
    public void testOnWebMessageReplyProxyClosed() throws Exception {
        // Load a page with a form.
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        assertNotNull(activity);
        CallbackHelper callbackHelper = new CallbackHelper();
        WebMessageCallbackImpl webMessageCallback = new WebMessageCallbackImpl(callbackHelper);
        runOnUiThreadBlocking(() -> {
            activity.getTab().registerWebMessageCallback(
                    webMessageCallback, "x", Arrays.asList("*"));
        });

        int majorVersion = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> WebLayer.getSupportedMajorVersion(mActivityTestRule.getActivity()));
        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestDataURL("web_message_test.html"));
        // web_message_test.html posts a message, wait for it.
        callbackHelper.waitForCallback(0);
        assertNotNull(webMessageCallback.mLastMessage);
        assertNotNull(webMessageCallback.mLastProxy);
        WebMessageReplyProxy proxy = webMessageCallback.mLastProxy;
        assertNull(webMessageCallback.mProxyClosed);
        assertFalse(
                runOnUiThreadBlocking(() -> { return webMessageCallback.mLastProxy.isClosed(); }));
        if (majorVersion >= 90) {
            assertTrue(runOnUiThreadBlocking(
                    () -> { return webMessageCallback.mLastProxy.isActive(); }));
        }
        webMessageCallback.reset();
        webMessageCallback.mClosedCallbackHelper = new CallbackHelper();

        // Navigate to a new page. The proxy should be closed.
        mActivityTestRule.navigateAndWait("about:blank");
        webMessageCallback.mClosedCallbackHelper.waitForFirst();
        assertNotNull(webMessageCallback.mProxyClosed);
        assertEquals(webMessageCallback.mProxyClosed, proxy);
        assertTrue(runOnUiThreadBlocking(() -> { return proxy.isClosed(); }));
        if (majorVersion >= 90) {
            assertFalse(runOnUiThreadBlocking(() -> { return proxy.isActive(); }));
        }
    }

    @Test
    @SmallTest
    public void testBadArguments() throws Exception {
        // Load a page with a form.
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        assertNotNull(activity);
        WebMessageCallback webMessageCallback = new WebMessageCallback() {};
        runOnUiThreadBlocking(() -> {
            // Invalid JS object name.
            try {
                activity.getTab().registerWebMessageCallback(
                        webMessageCallback, "", Arrays.asList("*"));
                fail();
            } catch (IllegalArgumentException e) {
            }

            // Origins can not have empty strings.
            try {
                activity.getTab().registerWebMessageCallback(
                        webMessageCallback, "x", Arrays.asList(""));
                fail();
            } catch (IllegalArgumentException e) {
            }

            // Origins can not be empty.
            try {
                activity.getTab().registerWebMessageCallback(
                        webMessageCallback, "x", new ArrayList<String>());
                fail();
            } catch (IllegalArgumentException e) {
            }

            // Invalid origin.
            try {
                activity.getTab().registerWebMessageCallback(
                        webMessageCallback, "x", Arrays.asList("***"));
                fail();
            } catch (IllegalArgumentException e) {
            }
        });
    }

    /* Disable BackForwardCacheMemoryControls to allow BackForwardCache for all devices regardless
     * of their memory. */
    @MinWebLayerVersion(90)
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=BackForwardCache", "disable-features=BackForwardCacheMemoryControls"})
    public void onActiveChangedForBackForwardCache() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        assertNotNull(activity);

        // Load a page with a message proxy and wait for the respond.
        CallbackHelper callbackHelper = new CallbackHelper();
        WebMessageCallbackImpl webMessageCallback = new WebMessageCallbackImpl(callbackHelper);
        String url = mActivityTestRule.getTestDataURL("web_message_test.html");
        int index = url.indexOf("/weblayer");
        assertNotEquals(-1, index);
        runOnUiThreadBlocking(() -> {
            activity.getTab().registerWebMessageCallback(
                    webMessageCallback, "x", Arrays.asList(url.substring(0, index)));
        });
        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestDataURL("web_message_test.html"));
        callbackHelper.waitForFirst();

        // There should be a proxy and it should be active.
        WebMessageReplyProxy proxy = webMessageCallback.mLastProxy;
        assertTrue(
                runOnUiThreadBlocking(() -> { return webMessageCallback.mLastProxy.isActive(); }));
        webMessageCallback.reset();
        webMessageCallback.mActiveChangedCallbackHelper = new CallbackHelper();

        // Navigate to a new page. The proxy should be inactive, but not closed.
        String url2 = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url2);
        webMessageCallback.mActiveChangedCallbackHelper.waitForCallback(0);
        assertFalse(runOnUiThreadBlocking(() -> { return proxy.isActive(); }));
        assertFalse(runOnUiThreadBlocking(() -> { return proxy.isClosed(); }));

        // Navigate back and ensure the page is active.
        runOnUiThreadBlocking(() -> { activity.getTab().getNavigationController().goBack(); });
        webMessageCallback.mActiveChangedCallbackHelper.waitForCallback(1);
        assertTrue(runOnUiThreadBlocking(() -> { return proxy.isActive(); }));
        assertFalse(runOnUiThreadBlocking(() -> { return proxy.isClosed(); }));

        // Post a message, to ensure the page can still get it.
        webMessageCallback.reset();
        webMessageCallback.mCallbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(() -> { proxy.postMessage(new WebMessage("2")); });
        webMessageCallback.mCallbackHelper.waitForFirst();
        assertNotNull(webMessageCallback.mLastMessage);
        assertEquals("bouncing 2", webMessageCallback.mLastMessage.getContents());
    }
}

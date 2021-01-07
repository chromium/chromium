// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
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
        WebMessageReplyProxy lastProxy = webMessageCallback.mLastProxy;
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
}

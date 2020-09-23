// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.ScrollOffsetCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test for ScrollOffsetCallback.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class ScrollOffsetCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    @MinWebLayerVersion(87)
    public void testBasic() throws Throwable {
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        // As registering for scroll offsets requires sending a message to the renderer, and
        // requires viz to be in a particular state, ensuring scrolls will generate a message is a
        // bit finicky. This tries to adjust the top-offset 10 times before giving up.
        CallbackHelper callbackHelper = new CallbackHelper();
        ScrollOffsetCallback scrollOffsetCallback = new ScrollOffsetCallback() {
            @Override
            public void onVerticalScrollOffsetChanged(int scrollOffset) {
                callbackHelper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().getActiveTab().registerScrollOffsetCallback(scrollOffsetCallback);
        });

        boolean gotInitialScroll = false;
        for (int i = 1; i < 10 && !gotInitialScroll; ++i) {
            mActivityTestRule.executeScriptSync("window.scroll(0, " + (i * 100) + ");", false);
            try {
                callbackHelper.waitForCallback(
                        "Waiting for initial scroll", 0, 1, 100, TimeUnit.MILLISECONDS);
                gotInitialScroll = true;
            } catch (TimeoutException e) {
            }
        }
        if (!gotInitialScroll) {
            Assert.fail("Was unable to get initial scroll, failing");
        }

        // Scroll back to the origin.
        CallbackHelper scrollBackCallbackHelper = new CallbackHelper();
        ScrollOffsetCallback scrollBackOffsetCallback = new ScrollOffsetCallback() {
            @Override
            public void onVerticalScrollOffsetChanged(int scrollOffset) {
                if (scrollOffset == 0) {
                    scrollBackCallbackHelper.notifyCalled();
                }
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().getActiveTab().registerScrollOffsetCallback(
                    scrollBackOffsetCallback);
        });
        mActivityTestRule.executeScriptSync("window.scroll(0, 0);", false);
        scrollBackCallbackHelper.waitForFirst();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().getActiveTab().unregisterScrollOffsetCallback(
                    scrollOffsetCallback);
        });

        // At this point scrollOffsetCallback is still registered. If this code were to remove the
        // callback the renderer might temporarily disable scroll-offsets notification, which
        // could potentially lead to raciness. In other words, best to leave it for future
        // assertions.
    }

    // NOTE: if adding another test, you'll likely want to make testBasic() a setUp type function
    // that is shared.
}

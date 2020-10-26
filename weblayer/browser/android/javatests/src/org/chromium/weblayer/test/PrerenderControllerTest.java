// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer;
import org.chromium.weblayer.PrerenderController;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.IOException;
import java.io.OutputStream;

/** Tests verifying PrerenderController behavior. */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class PrerenderControllerTest {
    private static final String DEFAULT_BODY = "<html><title>TestPage</title>Hello World!</html>";
    private CallbackHelper mPrerenderedPageFetched;

    private WebServer.RequestHandler mRequestHandler = new WebServer.RequestHandler() {
        @Override
        public void handleRequest(WebServer.HTTPRequest request, OutputStream stream) {
            try {
                if (request.getURI().contains("prerendered_page.html")) {
                    TestThreadUtils.runOnUiThreadBlocking(
                            () -> mPrerenderedPageFetched.notifyCalled());
                }
                WebServer.writeResponse(stream, WebServer.STATUS_OK, DEFAULT_BODY.getBytes());
            } catch (IOException exception) {
                Assert.fail(exception.getMessage()
                        + " \n while handling request: " + request.toString());
            }
        }
    };

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    @Feature({"WebLayer"})
    @MinWebLayerVersion(87)
    public void testAddingPrerender() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        testServer.setRequestHandler(mRequestHandler);
        mPrerenderedPageFetched = new CallbackHelper();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrerenderController prerenderController =
                    activity.getBrowser().getProfile().getPrerenderController();
            prerenderController.schedulePrerender(
                    Uri.parse(testServer.getResponseUrl("/prerendered_page.html")));
        });
        mPrerenderedPageFetched.waitForFirst();
    }
}

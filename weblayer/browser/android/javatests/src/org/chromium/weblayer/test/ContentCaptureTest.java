// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * Verifies Content Capture works in WebLayer. The feature itself has AwContentCaptureTest.java for
 * testing its functionality.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class ContentCaptureTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final String MAIN_FRAME_FILE = "/main_frame.html";

    private TestWebServer mWebServer;
    private InstrumentationActivity mActivity;

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() throws Exception {
        mWebServer.shutdown();
    }

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
    }

    /**
     * Verifies that ContentCapture is working for WebLayer as the TestContentCaptureConsumer is
     * receiving data from the renderer side.
     */
    @Test
    @SmallTest
    public void testContentCapture() throws Exception {
        final String response = "<html><head></head><body>"
                + "<div id='place_holder'>"
                + "<p style=\"height: 100vh\">Hello</p>"
                + "<p>world</p>"
                + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);

        ArrayList<Integer> eventsObserved = new ArrayList<>();
        CallbackHelper helper = new CallbackHelper();

        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestWebLayer testWebLayer = getTestWebLayer();
        testWebLayer.addContentCaptureConsumer(
                mActivity.getBrowser(), () -> helper.notifyCalled(), eventsObserved);

        mActivityTestRule.navigateAndWait(url);
        helper.waitForFirst();

        Assert.assertEquals(1, eventsObserved.size());
        Assert.assertEquals(/* CONTENT_CAPTURED*/ Integer.valueOf(1), eventsObserved.get(0));
    }

    /**
     * Verifies that ContentCapture doesn't report data for incognito mode.
     */
    @Test
    @SmallTest
    public void testContentCaptureIncognito() throws Exception {
        final String response = "<html><head></head><body>"
                + "<div id='place_holder'>"
                + "<p style=\"height: 100vh\">Hello</p>"
                + "<p>world</p>"
                + "</body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, response, null);

        ArrayList<Integer> eventsObserved = new ArrayList<>();
        CallbackHelper helper = new CallbackHelper();

        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank", extras);
        TestWebLayer testWebLayer = getTestWebLayer();
        testWebLayer.addContentCaptureConsumer(
                mActivity.getBrowser(), () -> helper.notifyCalled(), eventsObserved);

        mActivityTestRule.navigateAndWait(url);
        try {
            helper.waitForFirst();
        } catch (TimeoutException e) {
            // Expecting TimeoutException.
            return;
        }
        Assert.assertTrue("There should be a TimeoutException", false);
    }
}

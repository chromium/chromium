// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Assertions for WebLayer.registerExternalExperimentIDs().
 */
// The flags are necessary for the following reasons:
// host-resolver-rules: to make 'google.com' redirect to the port created by TestWebServer.
// ignore-certificate-errors: TestWebServer doesn't have a real cert.
@CommandLineFlags.Add({"host-resolver-rules='MAP * 127.0.0.1'", "ignore-certificate-errors"})
@RunWith(WebLayerJUnit4ClassRunner.class)
public class RegisterExternalExperimentIdsTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    /**
     * This is an end-to-end test of registerExternalExperimentIDs. It also covers
     * https://crbug.com/1147827.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(88) // Fix landed in 88.
    public void testRegisterExternalExperimentIds() throws Exception {
        mActivityTestRule.getTestServerRule().setServerUsesHttps(true);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        TestWebServer testServer = TestWebServer.startSsl();
        // Experiment ids are only added for certain domains, of which google is one.
        testServer.setServerHost("google.com");
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url);
        String initialValue = testServer.getLastRequest("/ok.html").headerValue("X-Client-Data");
        runOnUiThreadBlocking(() -> {
            mActivityTestRule.getWebLayer().registerExternalExperimentIDs(new int[] {1});
        });

        String url2 = testServer.setResponse("/ok2.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url2);
        String secondValue = testServer.getLastRequest("/ok2.html").headerValue("X-Client-Data");
        // The contents of the header are an encoded protobuf, which is a bit hard to verify in a
        // test. The key things to assert is registering an id changes the value, and that it's not
        // empty.
        assertFalse(secondValue.isEmpty());
        assertNotEquals(initialValue, secondValue);
    }
}

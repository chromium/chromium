// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Build;
import android.os.SystemClock;
import android.view.KeyEvent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;
import org.chromium.weblayer_private.test_interfaces.AutofillEventType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Verifies Autofill works in WebLayer. The feature itself has AwAutofillTest.java for testing its
 * functionality.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
public class AutofillTest {
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
     * Verifies that Autofill is working for WebLayer as the TestAutofillManagerWrapper is
     * receiving data from the renderer side.
     */
    @Test
    @SmallTest
    public void testBasicAutofill() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<label>User Name:</label>"
                + "<input type='text' id='text1' name='name' maxlength='30'"
                + " placeholder='placeholder@placeholder.com' autocomplete='name given-name'>"
                + "<input type='checkbox' id='checkbox1' name='showpassword'>"
                + "<select id='select1' name='month'>"
                + "<option value='1'>Jan</option>"
                + "<option value='2'>Feb</option>"
                + "</select><textarea id='textarea1'></textarea>"
                + "<div contenteditable id='div1'>hello</div>"
                + "<input type='submit'>"
                + "<input type='reset' id='reset1'>"
                + "<input type='color' id='color1'><input type='file' id='file1'>"
                + "<input type='image' id='image1'>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(MAIN_FRAME_FILE, data, null);

        ArrayList<Integer> eventsObserved = new ArrayList<>();
        CallbackHelper helper = new CallbackHelper();

        // Initialize the test shell, Browser and Tab objects should be created because they are
        // needed by the TestWebLayer#notifyOfAutofillEvents() method.
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestWebLayer testWebLayer = getTestWebLayer();
        testWebLayer.notifyOfAutofillEvents(
                mActivity.getBrowser(), () -> helper.notifyCalled(), eventsObserved);

        // Load the test page.
        mActivityTestRule.navigateAndWait(url);

        // Select the "text1" element.
        mActivityTestRule.executeScriptSync("document.getElementById('text1').select();", false);
        // Press "a" to trigger Autofill.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        List<Integer> expected = new ArrayList(Arrays.asList(AutofillEventType.VIEW_ENTERED,
                AutofillEventType.SESSION_STARTED, AutofillEventType.VALUE_CHANGED));
        // We don't have the cancel event on P+, but we will see them on O and OMR1.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            expected.add(0, AutofillEventType.CANCEL);
        }

        // Wait for Autofill events.
        helper.waitForCallback(
                /* currentCallCount */ 0, /* numberOfCallsToWaitFor */ expected.size(),
                CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        Assert.assertEquals(expected, eventsObserved);
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        long eventTime = SystemClock.uptimeMillis();
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, code, 0));
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP, code, 0));
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.dispatchKeyEvent(event));
    }
}

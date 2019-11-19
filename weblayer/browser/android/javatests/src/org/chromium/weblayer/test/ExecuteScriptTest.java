// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that script execution works as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ExecuteScriptTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><script>var bar = 10;</script></head><body>foo</body></html>");

    @Test
    @SmallTest
    public void testBasicScript() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        JSONObject result = mActivityTestRule.executeScriptSync(
                "document.body.innerHTML", true /* useSeparateIsolate */);
        Assert.assertEquals(result.getString(Tab.SCRIPT_RESULT_KEY), "foo");
    }

    @Test
    @SmallTest
    public void testScriptIsolatedFromPage() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        JSONObject result =
                mActivityTestRule.executeScriptSync("bar", true /* useSeparateIsolate */);
        Assert.assertTrue(result.isNull(Tab.SCRIPT_RESULT_KEY));
    }

    @Test
    @SmallTest
    public void testMainWorldScriptNotIsolatedFromPage() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        JSONObject result =
                mActivityTestRule.executeScriptSync("bar", false /* useSeparateIsolate */);
        Assert.assertEquals(result.getInt(Tab.SCRIPT_RESULT_KEY), 10);
    }

    @Test
    @SmallTest
    public void testScriptNotIsolatedFromOtherScript() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        mActivityTestRule.executeScriptSync("var foo = 20;", true /* useSeparateIsolate */);
        JSONObject result =
                mActivityTestRule.executeScriptSync("foo", true /* useSeparateIsolate */);
        Assert.assertEquals(result.getInt(Tab.SCRIPT_RESULT_KEY), 20);
    }

    @Test
    @SmallTest
    public void testClearedOnNavigate() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        mActivityTestRule.executeScriptSync("var foo = 20;", true /* useSeparateIsolate */);

        String newUrl = UrlUtils.encodeHtmlDataUri("<html></html>");
        mActivityTestRule.navigateAndWait(newUrl);
        JSONObject result =
                mActivityTestRule.executeScriptSync("foo", true /* useSeparateIsolate */);
        Assert.assertTrue(result.isNull(Tab.SCRIPT_RESULT_KEY));
    }

    @Test
    @SmallTest
    public void testNullCallback() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(DATA_URL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Null callback should not crash.
            activity.getTab().executeScript("null", true /* useSeparateIsolate */, null);
        });
        // Execute a sync script to make sure the other script finishes.
        mActivityTestRule.executeScriptSync("null", true /* useSeparateIsolate */);
    }
}

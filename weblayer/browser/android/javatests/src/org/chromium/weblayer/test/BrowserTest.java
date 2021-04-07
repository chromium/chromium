// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests for Browser.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class BrowserTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Test
    @SmallTest
    public void testDestroyTab() {
        String url = mActivityTestRule.getTestDataURL("before_unload.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Browser browser = mActivity.getBrowser();
            Tab tab = browser.getActiveTab();
            Assert.assertFalse(tab.isDestroyed());
            browser.destroyTab(tab);
            Assert.assertTrue(tab.isDestroyed());
        });
    }
}

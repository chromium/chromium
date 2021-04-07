// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.fragment.app.FragmentManager;
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

    private boolean isPageVisible() {
        return mActivityTestRule.executeScriptAndExtractBoolean(
                "document.visibilityState === 'visible'");
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(91)
    public void testSetChangeVisibilityOnNextDetach() {
        String url = mActivityTestRule.getTestDataURL("visibility.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        // Force 'gotHide' to initially be false.
        mActivityTestRule.executeScriptSync("gotHide = false;", false);

        // Force the page to be visible during detach, detach the Fragment, and ensure the page is
        // still visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getBrowser().setChangeVisibilityOnNextDetach(false);
            FragmentManager fm = mActivity.getSupportFragmentManager();
            fm.beginTransaction().detach(mActivityTestRule.getFragment()).commitNow();
        });
        Assert.assertFalse(mActivityTestRule.executeScriptAndExtractBoolean("gotHide", false));
        Assert.assertTrue(isPageVisible());

        // Attach the Fragment, the page should still be visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FragmentManager fm = mActivity.getSupportFragmentManager();
            fm.beginTransaction().attach(mActivityTestRule.getFragment()).commitNow();
        });
        Assert.assertFalse(mActivityTestRule.executeScriptAndExtractBoolean("gotHide", false));
        Assert.assertTrue(isPageVisible());

        // Detach the Fragment. Because setChangeVisibilityOnNextDetach() was reset as part of
        // attach, the page should no longer be visible and 'gotHide' should be true.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            FragmentManager fm = mActivity.getSupportFragmentManager();
            fm.beginTransaction().detach(mActivityTestRule.getFragment()).commitNow();
        });
        Assert.assertTrue(mActivityTestRule.executeScriptAndExtractBoolean("gotHide", false));
        Assert.assertFalse(isPageVisible());
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that NewTabCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class NewTabCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    private static final class CloseTabNewTabCallbackImpl extends NewTabCallback {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void onNewTab(Tab tab, int mode) {}

        @Override
        public void onCloseTab() {
            mCallbackHelper.notifyCalled();
        }

        public void waitForCloseTab() {
            try {
                // waitForFirst() only handles a single call. If you need more convert from
                // waitForFirst().
                mCallbackHelper.waitForFirst();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    @Test
    @SmallTest
    public void testNewBrowser() {
        String url = mActivityTestRule.getTestDataURL("new_browser.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        Tab firstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        callback.waitForNewTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(2, mActivity.getBrowser().getTabs().size());
            Tab secondTab = mActivity.getBrowser().getActiveTab();
            Assert.assertNotSame(firstTab, secondTab);
        });
    }

    @Test
    @SmallTest
    public void testCloseTab() {
        String url = mActivityTestRule.getTestDataURL("new_tab_then_close.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        Tab firstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        // Click on the tab to trigger creating a new tab.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        callback.waitForNewTab();
        CloseTabNewTabCallbackImpl closeTabImpl = new CloseTabNewTabCallbackImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(2, mActivity.getBrowser().getTabs().size());
            Tab secondTab = mActivity.getBrowser().getActiveTab();
            Assert.assertNotSame(firstTab, secondTab);
            secondTab.setNewTabCallback(closeTabImpl);
            // Switch to the first tab so clicking closes |secondTab|.
            secondTab.getBrowser().setActiveTab(firstTab);
        });

        // Clicking on the tab again to callback to close the tab.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        closeTabImpl.waitForCloseTab();
    }
}

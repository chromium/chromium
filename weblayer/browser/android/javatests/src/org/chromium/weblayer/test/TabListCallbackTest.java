// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests that NewTabCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabListCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private Tab mFirstTab;
    private Tab mSecondTab;

    private static class TabListCallbackImpl extends TabListCallback {
        public static final String ADDED = "added";
        public static final String ACTIVE = "active";
        public static final String REMOVED = "removed";

        private List<String> mObservedValues =
                Collections.synchronizedList(new ArrayList<String>());

        @Override
        public void onActiveTabChanged(Tab activeTab) {
            recordValue(ACTIVE);
        }

        @Override
        public void onTabAdded(Tab tab) {
            recordValue(ADDED);
        }

        @Override
        public void onTabRemoved(Tab tab) {
            recordValue(REMOVED);
        }

        private void recordValue(String parameter) {
            mObservedValues.add(parameter);
        }

        public List<String> getObservedValues() {
            return mObservedValues;
        }
    }

    @Before
    public void setUp() {
        String url = mActivityTestRule.getTestDataURL("new_browser.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        mFirstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        callback.waitForNewTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(2, mActivity.getBrowser().getTabs().size());
            mSecondTab = mActivity.getBrowser().getActiveTab();
            Assert.assertNotSame(mFirstTab, mSecondTab);
        });
    }

    @Test
    @SmallTest
    public void testActiveTabChanged() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabListCallbackImpl callback = new TabListCallbackImpl();
            mActivity.getBrowser().registerTabListCallback(callback);
            mActivity.getBrowser().setActiveTab(mFirstTab);
            Assert.assertTrue(callback.getObservedValues().contains(TabListCallbackImpl.ACTIVE));
        });
    }

    @Test
    @SmallTest
    public void testMoveToDifferentFragment() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Browser browser2 =
                    Browser.fromFragment(mActivity.createBrowserFragment(0, new Bundle()));
            Browser browser1 = mActivity.getBrowser();
            TabListCallbackImpl callback1 = new TabListCallbackImpl();
            browser1.registerTabListCallback(callback1);

            TabListCallbackImpl callback2 = new TabListCallbackImpl();
            browser2.registerTabListCallback(callback2);

            // Move the active tab from browser1 to browser2.
            Tab tabToMove = browser1.getActiveTab();
            browser2.addTab(tabToMove);
            // This should notify callback1 the active tab changed and a tab was removed.
            int browser1ActiveIndex =
                    callback1.getObservedValues().indexOf(TabListCallbackImpl.ACTIVE);
            Assert.assertNotSame(-1, browser1ActiveIndex);
            int browser1RemoveIndex =
                    callback1.getObservedValues().indexOf(TabListCallbackImpl.REMOVED);
            Assert.assertNotSame(-1, browser1RemoveIndex);
            Assert.assertTrue(browser1ActiveIndex < browser1RemoveIndex);
            Assert.assertSame(null, browser1.getActiveTab());
            Assert.assertEquals(1, browser1.getTabs().size());
            Assert.assertFalse(browser1.getTabs().contains(tabToMove));

            // callback2 should be notified of the insert.
            Assert.assertTrue(callback2.getObservedValues().contains(TabListCallbackImpl.ADDED));
            Assert.assertEquals(2, browser2.getTabs().size());
            Assert.assertTrue(browser2.getTabs().contains(tabToMove));
        });
    }

    @Test
    @SmallTest
    public void testDispose() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabListCallbackImpl callback = new TabListCallbackImpl();
            Browser browser = mActivity.getBrowser();
            browser.registerTabListCallback(callback);
            browser.destroyTab(mActivity.getBrowser().getActiveTab());
            Assert.assertTrue(callback.getObservedValues().contains(TabListCallbackImpl.ACTIVE));
            Assert.assertEquals(1, browser.getTabs().size());
        });
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;
import android.support.v4.app.Fragment;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Test for top-controls.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TopControlsTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private int mTopControlsHeight;
    private int mInitialVisiblePageHeight;
    private Tab mTab;
    private Browser mBrowser;

    /**
     * Returns the visible height of the page as determined by JS. The returned value is in CSS
     * pixels (which are most likely not the same as device pixels).
     */
    private int getVisiblePageHeight() {
        return mActivityTestRule.executeScriptAndExtractInt("window.innerHeight");
    }

    @Test
    @SmallTest
    public void testZeroHeight() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Fragment fragment = WebLayer.createBrowserFragment(null);
            activity.getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, fragment)
                    .commitNow();
            mBrowser = Browser.fromFragment(fragment);
            mBrowser.setTopView(new FrameLayout(activity));
            mTab = mBrowser.getActiveTab();
        });

        mActivityTestRule.navigateAndWait(mTab, UrlUtils.encodeHtmlDataUri("<html></html>"), true);

        // Calling setSupportsEmbedding() makes sure onTopControlsChanged() will get called, which
        // should not crash.
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBrowser.setSupportsEmbedding(true).addCallback(
                    (Boolean result) -> { helper.notifyCalled(); });
        });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testBasic() throws Exception {
        final String url = UrlUtils.encodeHtmlDataUri("<body><p style='height:5000px'>");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTopControlsHeight = activity.getTopContentsContainer().getHeight();
            Assert.assertTrue(mTopControlsHeight > 0);
            Assert.assertEquals(View.VISIBLE, activity.getTopContentsContainer().getVisibility());
        });

        // Get the size of the page.
        mInitialVisiblePageHeight = getVisiblePageHeight();
        Assert.assertTrue(mInitialVisiblePageHeight > 0);

        // Move by the size of the top-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopControlsHeight);

        // Moving should change the size of the page. Don't attempt to correlate the size as the
        // page doesn't see pixels, and to attempt to compare may result in rounding errors. Poll
        // for this value as there is no good way to detect when done.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mInitialVisiblePageHeight != getVisiblePageHeight();
            }
        });

        // Moving should also hide the top-controls View.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(View.INVISIBLE, activity.getTopContentsContainer().getVisibility());
        });

        // Move so top-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mTopControlsHeight);

        // Wait for the page height to match initial height.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mInitialVisiblePageHeight == getVisiblePageHeight();
            }
        });

        // top-controls are shown async.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity.getTopContentsContainer().getVisibility() == View.VISIBLE;
            }
        });
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.FindInPageCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests the behavior of FindInPageController and FindInPageCallback.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class FindInPageTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private CallbackImpl mCallback;

    private static class CallbackImpl extends FindInPageCallback {
        public int mNumberOfMatches;
        public int mActiveMatchIndex;
        public BoundedCountDownLatch mResultCountDown;
        public BoundedCountDownLatch mEndedCountDown;

        @Override
        public void onFindResult(int numberOfMatches, int activeMatchIndex, boolean finalUpdate) {
            mNumberOfMatches = numberOfMatches;
            mActiveMatchIndex = activeMatchIndex;
            if (finalUpdate) mResultCountDown.countDown();
        }

        @Override
        public void onFindEnded() {
            if (mEndedCountDown != null) mEndedCountDown.countDown();
        }
    }

    private void searchFor(String text, boolean forward) {
        mCallback.mResultCountDown = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().getFindInPageController().find(text, forward); });
        mCallback.mResultCountDown.timedAwait();
    }

    private void searchFor(String text) {
        searchFor(text, true);
    }

    private void verifyFindSessionInactive() {
        Tab tab = mActivity.getTab();
        // This verification only works on the active tab; if inactive setFindInPageCallback always
        // fails.
        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return tab == tab.getBrowser().getActiveTab(); }));

        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            // This call will return false if there's already a find session active.
            boolean settingCallbackWorked =
                    tab.getFindInPageController().setFindInPageCallback(new CallbackImpl());
            // Remove the new callback.
            tab.getFindInPageController().setFindInPageCallback(null);
            return settingCallbackWorked;
        }));
    }

    public void setUp(String testPage) {
        String url = mActivityTestRule.getTestDataURL(testPage);
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        mCallback = new CallbackImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().getFindInPageController().setFindInPageCallback(mCallback);
        });
    }

    @Test
    @SmallTest
    public void testBasic() {
        setUp("shakespeare.html");

        // Search.
        searchFor("de");
        Assert.assertEquals(mCallback.mNumberOfMatches, 5);
        Assert.assertEquals(mCallback.mActiveMatchIndex, 0);

        // Search again to activate the next match.
        searchFor("de");
        Assert.assertEquals(mCallback.mNumberOfMatches, 5);
        Assert.assertEquals(mCallback.mActiveMatchIndex, 1);

        // Search backwards to activate the previous match.
        searchFor("de", false);
        Assert.assertEquals(mCallback.mNumberOfMatches, 5);
        Assert.assertEquals(mCallback.mActiveMatchIndex, 0);

        // Add a character to narrow the search.
        searchFor("des");
        Assert.assertEquals(mCallback.mNumberOfMatches, 2);
        Assert.assertEquals(mCallback.mActiveMatchIndex, 0);
        searchFor("des");
        Assert.assertEquals(mCallback.mActiveMatchIndex, 1);

        // Simulate a backspace; the active match shouldn't change (although the number of results
        // and therefore indexing does change).
        searchFor("de");
        Assert.assertEquals(mCallback.mNumberOfMatches, 5);
        Assert.assertEquals(mCallback.mActiveMatchIndex, 4);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().getFindInPageController().setFindInPageCallback(null);
        });
        verifyFindSessionInactive();
    }

    @Test
    @SmallTest
    public void testHideOnNavigate() {
        setUp("shakespeare.html");

        mCallback.mEndedCountDown = new BoundedCountDownLatch(1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().getNavigationController().navigate(Uri.parse("simple_page.html"));
        });

        mCallback.mEndedCountDown.timedAwait();
        verifyFindSessionInactive();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1248187")
    public void testHideOnNewTab() {
        setUp("new_browser.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getBrowser().getActiveTab().setNewTabCallback(new NewTabCallbackImpl());
        });

        mCallback.mEndedCountDown = new BoundedCountDownLatch(1);

        // This touch creates a new tab.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());

        mCallback.mEndedCountDown.timedAwait();
    }
}

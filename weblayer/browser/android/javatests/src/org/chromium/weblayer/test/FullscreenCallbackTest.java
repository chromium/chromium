// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.FullscreenCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that FullscreenCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FullscreenCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private Delegate mDelegate;

    private static class Delegate extends FullscreenCallback {
        public int mEnterFullscreenCount;
        public int mExitFullscreenCount;
        public Runnable mExitFullscreenRunnable;

        @Override
        public void onEnterFullscreen(Runnable exitFullscreenRunner) {
            mEnterFullscreenCount++;
            mExitFullscreenRunnable = exitFullscreenRunner;
        }

        @Override
        public void onExitFullscreen() {
            mExitFullscreenCount++;
        }

        public void waitForFullscreen() {
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mEnterFullscreenCount == 1;
                }
            });
        }

        public void waitForExitFullscreen() {
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mExitFullscreenCount == 1;
                }
            });
        }
    }

    @Before
    public void setUp() {
        String url = mActivityTestRule.getTestDataURL("fullscreen.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        mDelegate = new Delegate();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setFullscreenCallback(mDelegate); });

        // First touch enters fullscreen.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForFullscreen();
        Assert.assertEquals(1, mDelegate.mEnterFullscreenCount);
    }

    @Test
    @SmallTest
    public void testFullscreen() {
        // Second touch exits.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    @Test
    @SmallTest
    public void testExitFullscreenWhenDelegateCleared() {
        // Clearing the FullscreenCallback should exit fullscreen.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setFullscreenCallback(null); });
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    @Test
    @SmallTest
    public void testExitFullscreenUsingRunnable() {
        // Running the runnable supplied to the delegate should exit fullscreen.
        TestThreadUtils.runOnUiThreadBlocking(mDelegate.mExitFullscreenRunnable);
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }
}

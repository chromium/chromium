// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that FullscreenCallback methods are invoked as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class FullscreenCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private TestFullscreenCallback mDelegate;

    @Before
    public void setUp() {
        String url = mActivityTestRule.getTestDataURL("fullscreen.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        mDelegate = new TestFullscreenCallback();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setFullscreenCallback(mDelegate); });

        // First touch enters fullscreen.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForFullscreen();
        Assert.assertEquals(1, mDelegate.mEnterFullscreenCount);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1133893")
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

    @Test
    @SmallTest
    public void testExitFullscreenWhenTabDestroyed() {
        // Destroying the tab should exit fullscreen.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().getBrowser().destroyTab(mActivity.getTab()); });
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    /**
     * Verifies there are no crashes when destroying the fragment in fullscreen.
     */
    @Test
    @SmallTest
    public void testDestroyFragmentWhileFullscreen() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivity.destroyFragment(); });
    }
}

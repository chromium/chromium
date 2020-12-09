// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.BrowserControlsOffsetCallback;
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

    // Launch WL and triggers html fullscreen.
    private void enterFullscren() {
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
        enterFullscren();
        // Second touch exits.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    @Test
    @SmallTest
    public void testExitFullscreenWhenDelegateCleared() {
        enterFullscren();
        // Clearing the FullscreenCallback should exit fullscreen.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setFullscreenCallback(null); });
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    @Test
    @SmallTest
    public void testExitFullscreenUsingRunnable() {
        enterFullscren();
        // Running the runnable supplied to the delegate should exit fullscreen.
        TestThreadUtils.runOnUiThreadBlocking(mDelegate.mExitFullscreenRunnable);
        mDelegate.waitForExitFullscreen();
        Assert.assertEquals(1, mDelegate.mExitFullscreenCount);
    }

    @Test
    @SmallTest
    public void testExitFullscreenWhenTabDestroyed() {
        enterFullscren();
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
        enterFullscren();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivity.destroyFragment(); });
    }

    // Waits for the top offset to go to -height. This means the view is completely hidden.
    private final class BrowserControlsOffsetCallbackImpl extends BrowserControlsOffsetCallback {
        private final CallbackHelper mCallbackHelper;
        BrowserControlsOffsetCallbackImpl(CallbackHelper callbackHelper) {
            mCallbackHelper = callbackHelper;
        }
        @Override
        public void onTopViewOffsetChanged(int offset) {
            int height = mActivity.getTopContentsContainer().getHeight();
            if (height != 0 && offset == -height) {
                mCallbackHelper.notifyCalled();
            }
        }
    }

    @Test
    @SmallTest
    public void testTopViewRemainsHiddenOnFullscreenRotation() throws Exception {
        String url = mActivityTestRule.getTestDataURL("rotation2.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        // Ensure the fragment is not recreated as otherwise things bounce around more.
        mActivityTestRule.setRetainInstance(true);
        Assert.assertNotNull(mActivity);
        mDelegate = new TestFullscreenCallback();
        CallbackHelper callbackHelper = new CallbackHelper();
        // The offsets may move around during rotation. Wait for reattachment before installing
        // the BrowserControlsOffsetCallback.
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser) {
                        browser.registerBrowserControlsOffsetCallback(
                                new BrowserControlsOffsetCallbackImpl(callbackHelper));
                    }
                });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setFullscreenCallback(mDelegate); });

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForFullscreen();
        Assert.assertEquals(1, mDelegate.mEnterFullscreenCount);

        // There should be a fullscreen element.
        Assert.assertTrue(mActivityTestRule.executeScriptAndExtractBoolean(
                "document.fullscreenElement != null"));

        // Rotation should trigger the view being totally hidden.
        callbackHelper.waitForFirst();
    }
}

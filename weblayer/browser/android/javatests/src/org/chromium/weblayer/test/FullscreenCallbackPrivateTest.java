// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.RemoteException;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.FullscreenCallback;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Fullscreen test assertions that can only be done using private API.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class FullscreenCallbackPrivateTest {
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
    }

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(
                mActivityTestRule.getActivity().getApplicationContext());
    }

    @Test
    @SmallTest
    public void testToastNotShownWhenFullscreenRequestIgnored() throws Throwable {
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlocking(
                () -> { return getTestWebLayer().didShowFullscreenToast(mActivity.getTab()); }));

        // Touch to enter fullscreen
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mDelegate.waitForFullscreen();
        Assert.assertEquals(1, mDelegate.mEnterFullscreenCount);

        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlocking(
                () -> { return getTestWebLayer().didShowFullscreenToast(mActivity.getTab()); }));
    }

    @Test
    @SmallTest
    public void testToastShown() throws Throwable {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().setFullscreenCallback(new FullscreenCallback() {
                @Override
                public void onEnterFullscreen(Runnable exitFullscreenRunner) {
                    mActivity.getWindow().getDecorView().setSystemUiVisibility(
                            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
                }

                @Override
                public void onExitFullscreen() {}
            });
        });

        // Touch to enter fullscreen
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        CriteriaHelper.pollUiThread(() -> {
            try {
                Criteria.checkThat(getTestWebLayer().didShowFullscreenToast(mActivity.getTab()),
                        Matchers.is(true));
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
    }
}

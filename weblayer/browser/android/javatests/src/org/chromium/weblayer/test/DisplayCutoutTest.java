// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Build;
import android.view.WindowManager.LayoutParams;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that viewport-fit is respected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class DisplayCutoutTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Before
    public void setUp() {
        String url = mActivityTestRule.getTestDataURL("display_cutout.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testWithFullscreen() {
        Assert.assertEquals(mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode,
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);

        // First touch enters fullscreen.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.executeScriptAndExtractBoolean("document.webkitIsFullScreen"),
                    Matchers.is(true));
        });

        mActivityTestRule.executeScriptSync("setViewportFit(\"contain\")", false);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode,
                    Matchers.is(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER));
        });
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testWithNoFullscreen() {
        Assert.assertEquals(mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode,
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        Assert.assertFalse(
                mActivityTestRule.executeScriptAndExtractBoolean("document.webkitIsFullScreen"));
        mActivityTestRule.executeScriptSync("setViewportFit(\"contain\")", false);

        try {
            // When not in fullscreen, this criterion will not be fulfilled.
            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat(mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode,
                        Matchers.not(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT));
            });
        } catch (AssertionError e) {
        }

        Assert.assertEquals(mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode,
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }
}

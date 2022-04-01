// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.inputmethod.InputMethodManager;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that the Web Contents is sized appropriately when in fullscreen and the onscreen keyboard
 * is shown.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class FullscreenSizeTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1239002")
    public void testOsk() {
        // For this test to function, it *cannot* use {@link TestFullscreenCallback}, as that
        // overrides the fullscreen handling in {@link InstrumentationActivity}.
        String url = mActivityTestRule.getTestDataURL("fullscreen_with_input.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);

        int initialHeight = getVisiblePageHeight();

        // First, try without fullscreen. Android should automatically resize the activity window.
        // First touch focuses input and brings up OSK (if it's enabled).
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        try {
            CriteriaHelper.pollInstrumentationThread(() -> {
                Criteria.checkThat(getVisiblePageHeight(), Matchers.lessThan(initialHeight));
            });
        } catch (AssertionError e) {
            // No soft keyboard found. This is possible when a hardware keyboard is attached.
            // Abort test.
            Assert.assertNotEquals(mActivity.getResources().getConfiguration().keyboard,
                    Configuration.KEYBOARD_NOKEYS);
            Assert.assertNotEquals(mActivity.getResources().getConfiguration().keyboard,
                    Configuration.KEYBOARD_UNDEFINED);
            return;
        }

        // Reset the OSK state.
        int withKeyboardHeight = getVisiblePageHeight();
        dismissOsk();
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.greaterThan(withKeyboardHeight));
        });

        // Now, try with fullscreen.
        // Second touch enters fullscreen.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.executeScriptAndExtractBoolean("document.webkitIsFullScreen"),
                    Matchers.is(true));
        });

        int fsWidth = getVisiblePageWidth();
        int fsHeight = getVisiblePageHeight();

        // Third touch focuses the input box, bringing up the on-screen keyboard.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        CriteriaHelper.pollInstrumentationThread(
                () -> { Criteria.checkThat(getVisiblePageHeight(), Matchers.lessThan(fsHeight)); });

        int fsWithOskWidth = getVisiblePageWidth();
        int fsWithOskHeight = getVisiblePageHeight();

        Assert.assertEquals(fsWithOskWidth, fsWidth);
        Assert.assertThat(fsWithOskHeight, Matchers.lessThan(fsHeight));

        dismissOsk();
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.greaterThan(fsWithOskHeight));
        });

        Assert.assertEquals(getVisiblePageWidth(), fsWidth);
        Assert.assertEquals(getVisiblePageHeight(), fsHeight);
    }

    private int getVisiblePageWidth() {
        return mActivityTestRule.executeScriptAndExtractInt("window.innerWidth");
    }

    private int getVisiblePageHeight() {
        return mActivityTestRule.executeScriptAndExtractInt("window.innerHeight");
    }

    private void dismissOsk() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InputMethodManager inputManager =
                    (InputMethodManager) mActivity.getSystemService(Activity.INPUT_METHOD_SERVICE);
            inputManager.hideSoftInputFromWindow(mActivity.getCurrentFocus().getWindowToken(), 0);
        });
    }
}

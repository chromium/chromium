// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Build.VERSION_CODES;
import android.widget.FrameLayout;

import androidx.fragment.app.Fragment;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Test for top-controls.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@CommandLineFlags.Add("enable-features=ImmediatelyHideBrowserControlsForTest")
public class TopControlsTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private Tab mTab;
    private Browser mBrowser;

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q,
            supported_abis_includes = "arm64-v8a", message = "crbug.com/1219507")
    public void
    testZeroHeight() throws Exception {
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mBrowser.setSupportsEmbedding(true, (result) -> helper.notifyCalled()); });
        helper.waitForCallback(0);
    }
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.RemoteException;
import android.view.View;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Helper class for tests that interact with browser controls. Such tests should add the following
 * above "public class FooTest":
 * @CommandLineFlags.Add("enable-features=ImmediatelyHideBrowserControlsForTest")
 */
public final class BrowserControlsHelper {
    private InstrumentationActivity mActivity;

    // Height of the top-view. Set in waitForBrowserControlsInitialization().
    private int mTopViewHeight;

    // Blocks until browser controls are fully initialized. Should only be created in a test's
    // setUp() method; see BrowserControlsHelper#createInSetUp().
    private BrowserControlsHelper(InstrumentationActivity activity) throws Exception {
        Assert.assertTrue(CommandLine.isInitialized());
        Assert.assertTrue(CommandLine.getInstance().hasSwitch("enable-features"));
        String enabledFeatures = CommandLine.getInstance().getSwitchValue("enable-features");
        Assert.assertTrue(enabledFeatures.contains("ImmediatelyHideBrowserControlsForTest"));

        mActivity = activity;

        waitForBrowserControlsInitialization();
    }

    private Tab getActiveTab() {
        return mActivity.getBrowser().getActiveTab();
    }

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
    }

    void waitForBrowserControlsViewToBeVisible(View v) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(v.getHeight(), Matchers.greaterThan(0));
            Criteria.checkThat(v.getVisibility(), Matchers.is(View.VISIBLE));
        });
    }

    // See TestWebLayer.waitForBrowserControlsMetadataState() for details on this.
    void waitForBrowserControlsMetadataState(int top, int bottom) throws Exception {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                getTestWebLayer().waitForBrowserControlsMetadataState(
                        getActiveTab(), top, bottom, () -> { helper.notifyCalled(); });
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
        helper.waitForFirst();
    }

    // Ensures that browser controls are fully initialized and ready for scrolls to be processed.
    private void waitForBrowserControlsInitialization() throws Exception {
        // Poll until the top view becomes visible.
        waitForBrowserControlsViewToBeVisible(mActivity.getTopContentsContainer());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTopViewHeight = mActivity.getTopContentsContainer().getHeight();
            Assert.assertTrue(mTopViewHeight > 0);
        });

        // Wait for cc to see the top-controls height.
        waitForBrowserControlsMetadataState(mTopViewHeight, 0);
    }

    // Creates a BrowserControlsHelper instance and blocks until browser controls are fully
    // initialized. Should be called from a test's setUp() method.
    static BrowserControlsHelper createAndBlockUntilBrowserControlsInitializedInSetUp(
            InstrumentationActivity activity) throws Exception {
        return new BrowserControlsHelper(activity);
    }

    // Returns the height of the top view.
    int getTopViewHeight() {
        return mTopViewHeight;
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that popup blocking works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class PopupTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private Context mRemoteContext;

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        Assert.assertNotNull(mActivity);

        mRemoteContext = TestWebLayer.getRemoteContext(mActivity.getApplicationContext());
    }

    @Test
    @SmallTest
    public void testOpenPopupFromInfoBar() throws Exception {
        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowser().getActiveTab().setNewTabCallback(callback); });

        // Try to open a popup.
        mActivityTestRule.executeScriptSync("window.open('about:blank')", true);

        // Make sure the infobar shows up and the popup has not been opened.
        String packageName =
                TestWebLayer.getWebLayerContext(mActivity.getApplicationContext()).getPackageName();
        int buttonId = ResourceUtil.getIdentifier(mRemoteContext, "id/button_primary", packageName);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mActivity.findViewById(buttonId), Matchers.notNullValue());
        });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertEquals(mActivity.getBrowser().getTabs().size(), 1); });

        // Click the button on the infobar to open the popup.
        EventUtils.simulateTouchCenterOfView(mActivity.findViewById(buttonId));
        callback.waitForNewTab();
    }

    @Test
    @SmallTest
    // This is a regression test for https://crbug.com/1142090 and verifies no crash when
    // NewTabCallback.onNewTab() destroys the supplied tab.
    public void testOpenPopupFromInfoBarAndNewTabCallbackDestroysTab() throws Exception {
        CallbackHelper helper = new CallbackHelper();
        NewTabCallback callback = new NewTabCallback() {
            @Override
            public void onNewTab(Tab tab, int mode) {
                tab.getBrowser().destroyTab(tab);
                helper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowser().getActiveTab().setNewTabCallback(callback); });

        // Try to open a popup.
        mActivityTestRule.executeScriptSync("window.open('about:blank')", true);

        // Make sure the infobar shows up and the popup has not been opened.
        String packageName =
                TestWebLayer.getWebLayerContext(mActivity.getApplicationContext()).getPackageName();
        int buttonId = ResourceUtil.getIdentifier(mRemoteContext, "id/button_primary", packageName);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mActivity.findViewById(buttonId), Matchers.notNullValue());
        });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertEquals(mActivity.getBrowser().getTabs().size(), 1); });

        // Click the button on the infobar to open the popup.
        EventUtils.simulateTouchCenterOfView(mActivity.findViewById(buttonId));

        // Wait for tab to be destroyed.
        helper.waitForFirst();
    }
}

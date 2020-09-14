// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Context;
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
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Basic tests to make sure WebLayer works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class TranslateTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private Context mRemoteContext;

    @Before
    public void setUp() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        mRemoteContext = TestWebLayer.getRemoteContext(mActivity.getApplicationContext());
        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
        testWebLayer.setIgnoreMissingKeyForTranslateManager(true);
        testWebLayer.forceNetworkConnectivityState(true);
    }

    @Test
    @SmallTest
    public void testCanTranslate() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mActivity.getBrowser().getActiveTab().canTranslate());
        });
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("fr_test.html"));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(mActivity.getBrowser().getActiveTab().canTranslate()); });
    }

    @Test
    @SmallTest
    public void testShowTranslateUi() {
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("fr_test.html"));
        waitForInfoBarToShow();

        EventUtils.simulateTouchCenterOfView(findViewByStringId("id/infobar_close_button"));
        waitForInfoBarToHide();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowser().getActiveTab().showTranslateUi(); });
        waitForInfoBarToShow();
    }

    private View findViewByStringId(String id) {
        return mActivity.findViewById(ResourceUtil.getIdentifier(mRemoteContext, id));
    }

    private void waitForInfoBarToShow() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(findViewByStringId("id/weblayer_translate_infobar_content"),
                    Matchers.notNullValue());
        });
    }

    private void waitForInfoBarToHide() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(findViewByStringId("id/weblayer_translate_infobar_content"),
                    Matchers.nullValue());
        });
    }
}

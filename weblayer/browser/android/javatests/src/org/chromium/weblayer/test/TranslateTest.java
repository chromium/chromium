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
import org.chromium.weblayer.Tab;
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
    private String mPackageName;

    @Before
    public void setUp() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        mRemoteContext = TestWebLayer.getRemoteContext(mActivity.getApplicationContext());
        mPackageName =
                TestWebLayer.getWebLayerContext(mActivity.getApplicationContext()).getPackageName();
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
    public void testShowTranslateUi() throws Exception {
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("fr_test.html"));
        waitForInfoBarToShow();
        Assert.assertEquals("English", getInfoBarTargetLanguage());

        EventUtils.simulateTouchCenterOfView(findViewByStringId("id/infobar_close_button"));
        waitForInfoBarToHide();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowser().getActiveTab().showTranslateUi(); });
        waitForInfoBarToShow();
    }

    @Test
    @SmallTest
    public void testOverridingOfTargetLanguage() throws Exception {
        // Sanity-check that by default the infobar appears with the target language of the user's
        // locale.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("french_page.html"));
        waitForInfoBarToShow();
        Assert.assertEquals("English", getInfoBarTargetLanguage());

        EventUtils.simulateTouchCenterOfView(findViewByStringId("id/infobar_close_button"));
        waitForInfoBarToHide();

        // Verify overriding of the target language.
        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.setTranslateTargetLanguage("de"); });
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("french_page.html"));
        waitForInfoBarToShow();
        Assert.assertEquals("German", getInfoBarTargetLanguage());

        EventUtils.simulateTouchCenterOfView(findViewByStringId("id/infobar_close_button"));
        waitForInfoBarToHide();

        // Check that the setting persists in the Tab by navigating to another page in French via a
        // link click.
        mActivityTestRule.executeScriptSync(
                "document.onclick = function() {document.getElementById('link_to_french_page2').click()}",
                true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());
        waitForInfoBarToShow();
        Assert.assertEquals("German", getInfoBarTargetLanguage());

        EventUtils.simulateTouchCenterOfView(findViewByStringId("id/infobar_close_button"));
        waitForInfoBarToHide();

        // Check that setting an empty string as the predefined target language causes behavior to
        // revert to default.
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.setTranslateTargetLanguage(""); });
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("french_page.html"));
        waitForInfoBarToShow();
        Assert.assertEquals("English", getInfoBarTargetLanguage());
    }

    private View findViewByStringId(String id) {
        return mActivity.findViewById(ResourceUtil.getIdentifier(mRemoteContext, id, mPackageName));
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

    private String getInfoBarTargetLanguage() throws Exception {
        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            return testWebLayer.getTranslateInfoBarTargetLanguage(
                    mActivity.getBrowser().getActiveTab());
        });
    }
}

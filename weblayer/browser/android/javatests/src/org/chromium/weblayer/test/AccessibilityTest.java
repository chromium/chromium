// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that accessibility works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class AccessibilityTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void testSetTextScaling() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("shakespeare.html"));
        int originalHeight = getTextHeight();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getTestWebLayer().setTextScaling(activity.getBrowser().getProfile(), 2.0f);
            return null;
        });
        assertThat(originalHeight).isLessThan(getTextHeight());
    }

    @Test
    @SmallTest
    public void testTextScalingSetsForceEnableZoom() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowser().getProfile();
            Assert.assertFalse(getTestWebLayer().getForceEnableZoom(profile));
            getTestWebLayer().setTextScaling(profile, 2.0f);
            Assert.assertTrue(getTestWebLayer().getForceEnableZoom(profile));
            return null;
        });
    }

    private int getTextHeight() {
        return mActivityTestRule.executeScriptAndExtractInt(
                "document.querySelector('p').clientHeight");
    }

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(
                mActivityTestRule.getActivity().getApplicationContext());
    }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.DarkModeStrategy;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that dark mode is handled correctly.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class DarkModeTest {
    private InstrumentationActivity mActivity;

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private void setDarkModeStrategy(@DarkModeStrategy int darkModeStrategy) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.loadWebLayerSync(mActivityTestRule.getContextForWebLayer());
            mActivity.getBrowser().setDarkModeStrategy(darkModeStrategy);
        });
    }

    private boolean loadPageAndGetPrefersDark() {
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("dark_mode.html"));
        return mActivityTestRule.executeScriptAndExtractBoolean(
                "window.matchMedia('(prefers-color-scheme: dark)').matches");
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(90)
    public void testDarkModeWithWebThemeDarkening() throws Exception {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        mActivity = mActivityTestRule.launchShell(new Bundle());
        setDarkModeStrategy(DarkModeStrategy.WEB_THEME_DARKENING_ONLY);
        boolean prefersDark = loadPageAndGetPrefersDark();
        Assert.assertTrue(prefersDark);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(90)
    public void testDarkModeWithUserAgentDarkening() throws Exception {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        mActivity = mActivityTestRule.launchShell(new Bundle());
        setDarkModeStrategy(DarkModeStrategy.USER_AGENT_DARKENING_ONLY);
        boolean prefersDark = loadPageAndGetPrefersDark();
        Assert.assertFalse(prefersDark);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(90)
    public void testDarkModeWithPreferWebThemeDarkening() throws Exception {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        mActivity = mActivityTestRule.launchShell(new Bundle());
        setDarkModeStrategy(DarkModeStrategy.PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING);
        boolean prefersDark = loadPageAndGetPrefersDark();
        Assert.assertTrue(prefersDark);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(90)
    public void testLightModeWithWebThemeDarkening() throws Exception {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
        mActivity = mActivityTestRule.launchShell(new Bundle());
        setDarkModeStrategy(DarkModeStrategy.WEB_THEME_DARKENING_ONLY);
        boolean prefersDark = loadPageAndGetPrefersDark();
        Assert.assertFalse(prefersDark);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(90)
    public void testLightModeWithUserAgentDarkening() throws Exception {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
        mActivity = mActivityTestRule.launchShell(new Bundle());
        setDarkModeStrategy(DarkModeStrategy.USER_AGENT_DARKENING_ONLY);
        boolean prefersDark = loadPageAndGetPrefersDark();
        Assert.assertFalse(prefersDark);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(90)
    public void testLightModeWithPreferWebThemeDarkening() throws Exception {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
        mActivity = mActivityTestRule.launchShell(new Bundle());
        setDarkModeStrategy(DarkModeStrategy.PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING);
        boolean prefersDark = loadPageAndGetPrefersDark();
        Assert.assertFalse(prefersDark);
    }
}

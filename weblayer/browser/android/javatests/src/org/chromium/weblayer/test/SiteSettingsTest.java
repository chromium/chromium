// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.StringContains.containsString;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.os.RemoteException;
import android.provider.Settings;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.weblayer.SettingsTestUtils;
import org.chromium.weblayer.SiteSettingsActivity;
import org.chromium.weblayer.TestWebLayer;

/**
 * Tests the behavior of the Site Settings UI.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class SiteSettingsTest {
    // google.com is the default search engine, which should always be in the
    // list of sites in "All Sites".
    private static final String GOOGLE_URL = "https://www.google.com";
    private static final String PROFILE_NAME = "DefaultProfile";

    @Rule
    public SettingsActivityTestRule mSettingsTestRule = new SettingsActivityTestRule();

    @Test
    @SmallTest
    public void testSiteSettingsLaunches() throws InterruptedException {
        mSettingsTestRule.launchActivity(
                SiteSettingsActivity.createIntentForSiteSettingsCategoryList(
                        mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false));

        onView(withText("All sites")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testAllSitesLaunches() throws InterruptedException {
        mSettingsTestRule.launchActivity(
                SiteSettingsActivity.createIntentForSiteSettingsCategoryList(
                        mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false));

        onView(withText("All sites")).perform(click());

        // Check that the google.com item is visible.
        onView(withText(GOOGLE_URL)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testJavascriptExceptionPopupLaunches() throws InterruptedException {
        mSettingsTestRule.launchActivity(
                SiteSettingsActivity.createIntentForSiteSettingsCategoryList(
                        mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false));

        onView(withText("JavaScript")).perform(click());
        onView(withText("Add site exception")).perform(click());

        onView(withText("Block JavaScript for a specific site.")).check(matches(isDisplayed()));
    }

    @DisabledTest(message = "https://crbug.com/1174618")
    @Test
    @SmallTest
    public void testAdBlockingSiteSettingPageLaunches() throws InterruptedException {
        // The setting for ad blocking is below the fold on the main site settings page, and it's
        // challenging to scroll to it. Launch directly to the category instead. See the discussion
        // on https://chromium-review.googlesource.com/c/chromium/src/+/2673520 for further details.
        // Note that this means that this test unfortunately doesn't verify that the Ads setting is
        // actually present on the main site settings page.
        mSettingsTestRule.launchActivity(
                SettingsTestUtils.createIntentForSiteSettingsSingleCategory(
                        mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false, "ads",
                        "Ads"));

        onView(withText("Block ads on sites that show intrusive or misleading ads"))
                .perform(click());

        onView(withText("Allowed")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSingleSiteSoundToggle() throws InterruptedException {
        mSettingsTestRule.launchActivity(SettingsTestUtils.createIntentForSiteSettingsSingleWebsite(
                mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false, GOOGLE_URL));

        onView(withText("Allowed")).check(matches(isDisplayed()));

        onView(withText("Sound")).perform(click());

        onView(withText("Blocked")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSingleSiteClearPopupLaunches() throws InterruptedException {
        mSettingsTestRule.launchActivity(SettingsTestUtils.createIntentForSiteSettingsSingleWebsite(
                mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false, GOOGLE_URL));

        onView(withText("Clear & reset")).perform(click());

        onView(withText(containsString("Are you sure you want to clear all local data")))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSingleSiteLocationAccess() throws InterruptedException, RemoteException {
        try {
            Intents.init();
            TestWebLayer testWebLayer =
                    TestWebLayer.getTestWebLayer(mSettingsTestRule.getContext());
            testWebLayer.setSystemLocationSettingEnabled(true);
            mSettingsTestRule.launchActivity(
                    SettingsTestUtils.createIntentForSiteSettingsSingleWebsite(
                            mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false,
                            GOOGLE_URL));

            Matcher<Intent> settingsMatcher =
                    IntentMatchers.hasAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            intending(settingsMatcher)
                    .respondWith(new ActivityResult(Activity.RESULT_OK, new Intent()));

            onView(withText("Location")).perform(click());

            intended(settingsMatcher);
        } finally {
            Intents.release();
        }
    }
}

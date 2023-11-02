// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.StringContains.containsString;

import android.os.RemoteException;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.SettingsTestUtils;
import org.chromium.weblayer.SiteSettingsActivity;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.WebLayer;

/**
 * Tests the behavior of the Site Settings UI.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class SiteSettingsTest {
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
    public void testAllSitesLaunches() throws Exception {
        mSettingsTestRule.launchActivity(
                SiteSettingsActivity.createIntentForSiteSettingsCategoryList(
                        mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false));
        TestWebLayer.getTestWebLayer(mSettingsTestRule.getContext())
                .grantLocationPermission(GOOGLE_URL);

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
        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(mSettingsTestRule.getContext());
        testWebLayer.setSystemLocationSettingEnabled(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebLayer.loadSync(mSettingsTestRule.getContext()).getProfile(PROFILE_NAME);
        });
        testWebLayer.grantLocationPermission(GOOGLE_URL);
        mSettingsTestRule.launchActivity(SettingsTestUtils.createIntentForSiteSettingsSingleWebsite(
                mSettingsTestRule.getContext(), PROFILE_NAME, /*isIncognito=*/false, GOOGLE_URL));

        onView(withText("Location")).perform(click());

        onView(withText("Blocked")).check(matches(isDisplayed()));
    }
}

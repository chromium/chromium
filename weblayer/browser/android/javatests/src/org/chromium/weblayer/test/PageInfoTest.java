// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.PageInfoDisplayOptions;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests the behavior of the Page Info UI.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class PageInfoTest {
    private static final String CONNECTION_IS_NOT_SECURE_TEXT = "Connection is not secure";

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private ViewInteraction onViewWaiting(Matcher<View> matcher) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                onView(matcher).check(matches(isDisplayed()));
            } catch (Error e) {
                throw new CriteriaNotSatisfiedException(e.toString());
            }
        });
        return onView(matcher);
    }

    @Test
    @SmallTest
    public void testPageInfoLaunches() {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_TEXT_CLICKABLE, false);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("simple_page.html"), extras);

        Context remoteContext = TestWebLayer.getRemoteContext(activity.getApplicationContext());
        String packageName =
                TestWebLayer.getWebLayerContext(activity.getApplicationContext()).getPackageName();
        int buttonId = ResourceUtil.getIdentifier(remoteContext, "id/security_button", packageName);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                EventUtils.simulateTouchCenterOfView(activity.findViewById(buttonId));
            }
        });
        onViewWaiting(withText(CONNECTION_IS_NOT_SECURE_TEXT));
    }

    @Test
    @SmallTest
    public void testShowPageInfo() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("simple_page.html"));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                PageInfoDisplayOptions options =
                        PageInfoDisplayOptions.builder().showPublisherUrl().build();
                activity.getBrowser().getUrlBarController().showPageInfo(options);
            }
        });
        onViewWaiting(withText(CONNECTION_IS_NOT_SECURE_TEXT));
    }

    @Test
    @SmallTest
    public void testSingleTappableContainer() {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_TEXT_CLICKABLE, true);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("simple_page.html"), extras);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                EventUtils.simulateTouchCenterOfView(activity.getUrlBarView());
            }
        });
        onViewWaiting(withText(CONNECTION_IS_NOT_SECURE_TEXT));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1312569")
    public void testPageInfoConnectionSubPage() {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_TEXT_CLICKABLE, true);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("simple_page.html"), extras);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                EventUtils.simulateTouchCenterOfView(activity.getUrlBarView());
            }
        });
        onViewWaiting(withText(CONNECTION_IS_NOT_SECURE_TEXT)).perform(click());
        onViewWaiting(withText("The identity of this website isn't verified."));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1188735")
    public void testPageInfoCookiesSubPage() {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_TEXT_CLICKABLE, true);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("simple_page.html"), extras);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                EventUtils.simulateTouchCenterOfView(activity.getUrlBarView());
            }
        });
        onViewWaiting(withText("Cookies")).perform(click());
        onViewWaiting(withText("0 cookies in use"));
    }
}

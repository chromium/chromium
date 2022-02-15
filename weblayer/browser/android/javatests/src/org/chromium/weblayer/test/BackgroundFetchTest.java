// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.service.notification.StatusBarNotification;

import androidx.annotation.RequiresApi;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.OpenUrlCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests Background Fetch and the OpenUrlCallback API.
 */
@MinWebLayerVersion(91)
@RunWith(WebLayerJUnit4ClassRunner.class)
public class BackgroundFetchTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
    }

    @Before
    public void setUp() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestServer().getURL(
                "/weblayer/test/data/background_fetch/index.html"));
        getTestWebLayer().expediteDownloadService();
    }

    @Test
    @LargeTest
    @FlakyTest(message = "https://crbug.com/1272010")
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void basic() throws Exception {
        Browser browser = mActivity.getBrowser();

        int numTabs = runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(1, numTabs);

        CallbackHelper tabAddedCallback = new CallbackHelper();
        OpenUrlCallback openUrlCallback = new OpenUrlCallback() {
            @Override
            public Browser getBrowserForNewTab() {
                return browser;
            }

            @Override
            public void onTabAdded(Tab tab) {
                tabAddedCallback.notifyCalled();
            }
        };

        runOnUiThreadBlocking(
                () -> { browser.getProfile().setTablessOpenUrlCallback(openUrlCallback); });

        Assert.assertNull(getBackgroundFetchNotification(null));

        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());

        // Wait for the notification to appear.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getBackgroundFetchNotification(null), Matchers.notNullValue());
        }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL * 2, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        // This part of the test passes locally but fails on some bots because the download never
        // completes. TODO(estade): this also fails now that permissions are extra strict, so
        // enabling this would require side-stepping permissions for this test. See
        // crbug.com/1189247
        //
        // Wait for the notification to indicate completion (also testing the page receives the
        // success message).
        // CriteriaHelper.pollInstrumentationThread(() -> {
        //    Criteria.checkThat(
        //            getBackgroundFetchNotification("New Fetched Title!"),
        //            Matchers.notNullValue());
        // }, 30000, 100);

        // -1 should be the ID of the first notification.
        getTestWebLayer().activateBackgroundFetchNotification(-1);

        // Tapping should add a new tab.
        tabAddedCallback.waitForFirst();

        numTabs = runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(2, numTabs);
    }

    /**
     * Retrieves the first active background fetch notification it finds, or null if none exists.
     *
     *
     * {@link NotificationManager#getActiveNotifications()} is only available from M.
     *
     * @param expectedTitle The title of the notification in question, or null if any notification
     *         will do.
     * @return The matched notification, or null.
     */
    @RequiresApi(Build.VERSION_CODES.M)
    private Notification getBackgroundFetchNotification(String expectedTitle) {
        StatusBarNotification notifications[] =
                ((NotificationManager) mActivity.getApplicationContext().getSystemService(
                         Context.NOTIFICATION_SERVICE))
                        .getActiveNotifications();
        for (StatusBarNotification statusBarNotification : notifications) {
            if (statusBarNotification.getTag().equals("org.chromium.weblayer.downloads")) {
                Notification notification = statusBarNotification.getNotification();
                if (expectedTitle == null) return notification;

                String title = notification.extras.getString(Notification.EXTRA_TITLE);
                if (title != null && title.contains(expectedTitle)) return notification;
            }
        }
        return null;
    }
}

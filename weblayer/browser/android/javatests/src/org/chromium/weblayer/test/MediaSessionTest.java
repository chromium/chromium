// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.chromium.weblayer.R.id.weblayer_media_session_notification;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.service.notification.StatusBarNotification;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that MediaSession works as expected.
 */
@CommandLineFlags.Add({"ignore-certificate-errors"})
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class MediaSessionTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void basic() throws Throwable {
        mActivity = mActivityTestRule.launchShellWithUrl(
                mActivityTestRule.getTestDataURL("media_session.html"));
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());

        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getMediaSessionNotification(), Matchers.notNullValue());
        });
    }

    /**
     * Retrieves the active media session notification, or null if none exists.
     * {@link NotificationManager#getActiveNotifications()} is only available from M.
     */
    @RequiresApi(Build.VERSION_CODES.M)
    private Notification getMediaSessionNotification() {
        StatusBarNotification notifications[] =
                ((NotificationManager) mActivity.getApplicationContext().getSystemService(
                         Context.NOTIFICATION_SERVICE))
                        .getActiveNotifications();
        for (StatusBarNotification statusBarNotification : notifications) {
            if (statusBarNotification.getId() == weblayer_media_session_notification) {
                return statusBarNotification.getNotification();
            }
        }
        return null;
    }
}

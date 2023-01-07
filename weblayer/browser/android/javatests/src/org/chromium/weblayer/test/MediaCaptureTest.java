// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.service.notification.StatusBarNotification;
import android.support.test.InstrumentationRegistry;
import android.webkit.ValueCallback;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.weblayer.MediaCaptureCallback;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that Media Capture and Streams Web API (MediaStream) works as expected.
 */
@CommandLineFlags.Add({"ignore-certificate-errors"})
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class MediaCaptureTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private TestWebLayer mTestWebLayer;
    private CallbackImpl mCaptureCallback;

    private static class CallbackImpl extends MediaCaptureCallback {
        boolean mAudio;
        boolean mVideo;
        public BoundedCountDownLatch mRequestedCountDown;
        public BoundedCountDownLatch mStateCountDown;

        @Override
        public void onMediaCaptureRequested(
                boolean audio, boolean video, ValueCallback<Boolean> requestResult) {
            requestResult.onReceiveValue(true);
            mRequestedCountDown.countDown();
        }

        @Override
        public void onMediaCaptureStateChanged(boolean audio, boolean video) {
            mAudio = audio;
            mVideo = video;
            mStateCountDown.countDown();
        }
    }

    @Before
    public void setUp() throws Throwable {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        Assert.assertNotNull(mActivity);

        mTestWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
        mActivityTestRule.getTestServerRule().setServerUsesHttps(true);

        mCaptureCallback = new CallbackImpl();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().getMediaCaptureController().setMediaCaptureCallback(
                    mCaptureCallback);
        });
    }

    /**
     * Basic test for a stream that includes audio and video.
     */
    @Test
    @MediumTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1107380
    public void basic() throws Throwable {
        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/weblayer/test/data/getusermedia.html"));

        grantPermissionAndWaitForStreamToStart();

        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertTrue(mCaptureCallback.mVideo);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            CriteriaHelper.pollInstrumentationThread(() -> {
                Criteria.checkThat(getMediaCaptureNotification(), Matchers.notNullValue());
            });
        }

        stopStream();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            CriteriaHelper.pollInstrumentationThread(() -> {
                Criteria.checkThat(getMediaCaptureNotification(), Matchers.nullValue());
            });
        }
    }

    /**
     * Tests that the per-site permission, once granted, is remembered the next time a stream is
     * requested.
     */
    @Test
    @MediumTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1107380
    public void rememberPermission() throws Throwable {
        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/weblayer/test/data/getusermedia.html"));

        grantPermissionAndWaitForStreamToStart();

        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertTrue(mCaptureCallback.mVideo);

        stopStream();

        // No permission prompt required the second time.
        mCaptureCallback.mRequestedCountDown = new BoundedCountDownLatch(1);
        mCaptureCallback.mStateCountDown = new BoundedCountDownLatch(1);
        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/weblayer/test/data/getusermedia.html"));
        mCaptureCallback.mRequestedCountDown.timedAwait();
        mCaptureCallback.mStateCountDown.timedAwait();

        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertTrue(mCaptureCallback.mVideo);
    }

    /**
     * Tests that a site can request two parallel streams and both are stopped via {@link
     * stopMediaCapturing}.
     */
    @Test
    @MediumTest
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1107380
    public void twoStreams() throws Throwable {
        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/weblayer/test/data/getusermedia2.html"));

        // Audio stream.
        grantPermissionAndWaitForStreamToStart();
        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertFalse(mCaptureCallback.mVideo);

        // Video stream.
        grantPermissionAndWaitForStreamToStart();
        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertTrue(mCaptureCallback.mVideo);

        mCaptureCallback.mStateCountDown = new BoundedCountDownLatch(2);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().getMediaCaptureController().stopMediaCapturing(); });
        mCaptureCallback.mStateCountDown.timedAwait();
        Assert.assertFalse(mCaptureCallback.mAudio);
        Assert.assertFalse(mCaptureCallback.mVideo);
    }

    /**
     * Tests that the notification posted for a tab will be updated if a second stream is started.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @DisableIf.Device(type = {UiDisableIf.TABLET}) // https://crbug.com/1107380
    public void twoStreamsNotification() throws Throwable {
        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/weblayer/test/data/getusermedia2.html"));

        // Audio stream.
        grantPermissionAndWaitForStreamToStart();
        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertFalse(mCaptureCallback.mVideo);

        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getMediaCaptureNotification(), Matchers.notNullValue());
        });
        Notification audioNotification = getMediaCaptureNotification();

        // Video stream.
        grantPermissionAndWaitForStreamToStart();
        Assert.assertTrue(mCaptureCallback.mAudio);
        Assert.assertTrue(mCaptureCallback.mVideo);

        CriteriaHelper.pollInstrumentationThread(() -> {
            Notification combinedNotification = getMediaCaptureNotification();
            Criteria.checkThat(combinedNotification, Matchers.notNullValue());
            Criteria.checkThat(combinedNotification.getSmallIcon().getResId(),
                    Matchers.not(audioNotification.getSmallIcon().getResId()));
        });

        mCaptureCallback.mStateCountDown = new BoundedCountDownLatch(2);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().getMediaCaptureController().stopMediaCapturing(); });
        mCaptureCallback.mStateCountDown.timedAwait();
        Assert.assertFalse(mCaptureCallback.mAudio);
        Assert.assertFalse(mCaptureCallback.mVideo);

        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(getMediaCaptureNotification(), Matchers.nullValue()));
    }

    private void grantPermissionAndWaitForStreamToStart() throws Throwable {
        CriteriaHelper.pollInstrumentationThread(
                () -> { return mTestWebLayer.isPermissionDialogShown(); });
        mCaptureCallback.mRequestedCountDown = new BoundedCountDownLatch(1);
        mCaptureCallback.mStateCountDown = new BoundedCountDownLatch(1);
        mTestWebLayer.clickPermissionDialogButton(true);

        mCaptureCallback.mRequestedCountDown.timedAwait();
        mCaptureCallback.mStateCountDown.timedAwait();
    }

    private void stopStream() throws Throwable {
        mCaptureCallback.mStateCountDown = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().getMediaCaptureController().stopMediaCapturing(); });
        mCaptureCallback.mStateCountDown.timedAwait();
        Assert.assertFalse(mCaptureCallback.mAudio);
        Assert.assertFalse(mCaptureCallback.mVideo);
    }

    /**
     * Retrieves the active media capture notification, or null if none exists.
     * Asserts that at most one notification exists.
     * {@link NotificationManager#getActiveNotifications()} is only available from M.
     */
    @RequiresApi(Build.VERSION_CODES.M)
    private Notification getMediaCaptureNotification() {
        StatusBarNotification notifications[];
        try {
            // Workaround for Android bug fixed in 34a80841cb8fa8cdbe6c584831f0e531618d331d.
            notifications =
                    ((NotificationManager) mActivity.getApplicationContext().getSystemService(
                             Context.NOTIFICATION_SERVICE))
                            .getActiveNotifications();
        } catch (NullPointerException e) {
            return null;
        }
        Notification notification = null;
        for (StatusBarNotification statusBarNotification : notifications) {
            if (statusBarNotification.getTag().equals("org.chromium.weblayer.webrtc.avstream")) {
                Assert.assertNull(notification);
                notification = statusBarNotification.getNotification();
                Assert.assertNotNull(notification.getSmallIcon().loadDrawable(
                        InstrumentationRegistry.getContext()));
            }
        }
        return notification;
    }
}

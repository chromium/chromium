// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.util.Pair;
import android.webkit.ValueCallback;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Download;
import org.chromium.weblayer.DownloadCallback;
import org.chromium.weblayer.DownloadError;
import org.chromium.weblayer.DownloadState;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Tests that the DownloadCallback method is invoked for downloads.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class DownloadCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static boolean sIsFileNameSupported;

    private InstrumentationActivity mActivity;
    private Callback mCallback;

    private static class Callback extends DownloadCallback {
        public String mUrl;
        public String mUserAgent;
        public String mContentDisposition;
        public String mMimetype;
        public String mLocation;
        public String mFileName;
        public @DownloadState int mState;
        public @DownloadError int mError;
        public long mContentLength;
        public boolean mIntercept;
        public boolean mSeenStarted;
        public boolean mSeenCompleted;
        public boolean mSeenFailed;

        @Override
        public boolean onInterceptDownload(Uri uri, String userAgent, String contentDisposition,
                String mimetype, long contentLength) {
            mUrl = uri.toString();
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimetype = mimetype;
            mContentLength = contentLength;
            return mIntercept;
        }

        @Override
        public void allowDownload(Uri uri, String requestMethod, Uri requestInitiator,
                ValueCallback<Boolean> callback) {
            callback.onReceiveValue(true);
        }

        @Override
        public void onDownloadStarted(Download download) {
            mSeenStarted = true;
            download.disableNotification();
        }

        @Override
        public void onDownloadCompleted(Download download) {
            mSeenCompleted = true;
            mLocation = download.getLocation().toString();
            if (sIsFileNameSupported) {
                mFileName = download.getFileNameToReportToUser().toString();
            }
            mState = download.getState();
            mError = download.getError();
            mMimetype = download.getMimeType();
        }

        @Override
        public void onDownloadFailed(Download download) {
            mSeenFailed = true;
            mState = download.getState();
            mError = download.getError();
        }

        public void waitForIntercept() {
            CriteriaHelper.pollInstrumentationThread(
                    () -> Criteria.checkThat(mUrl, Matchers.notNullValue()));
        }

        public void waitForStarted() {
            CriteriaHelper.pollInstrumentationThread(() -> mSeenStarted);
        }

        public void waitForCompleted() {
            CriteriaHelper.pollInstrumentationThread(() -> mSeenCompleted);
        }

        public void waitForFailed() {
            CriteriaHelper.pollInstrumentationThread(() -> mSeenFailed);
        }
    }

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchShellWithUrl(null);
        Assert.assertNotNull(mActivity);

        // Don't fill up the default download directory on the device.
        String tempDownloadDirectory =
                InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir()
                + "/weblayer/Downloads";

        mCallback = new Callback();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = mActivity.getBrowser().getProfile();
            profile.setDownloadCallback(mCallback);
            profile.setDownloadDirectory(new File(tempDownloadDirectory));

            sIsFileNameSupported =
                    WebLayer.getSupportedMajorVersion(mActivity.getApplicationContext()) >= 86;
        });
    }

    /**
     * Verifies the DownloadCallback is informed of downloads resulting from navigations to pages
     * with Content-Disposition attachment.
     */
    @Test
    @SmallTest
    public void testInterceptDownloadByContentDisposition() throws Throwable {
        mCallback.mIntercept = true;
        final String data = "download data";
        final String contentDisposition = "attachment;filename=\"download.txt\"";
        final String mimetype = "text/plain";

        List<Pair<String, String>> downloadHeaders = new ArrayList<Pair<String, String>>();
        downloadHeaders.add(Pair.create("Content-Disposition", contentDisposition));
        downloadHeaders.add(Pair.create("Content-Type", mimetype));
        downloadHeaders.add(Pair.create("Content-Length", Integer.toString(data.length())));

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse("/download.txt", data, downloadHeaders);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mActivity.getTab().getNavigationController().navigate(Uri.parse(pageUrl));
            });
            mCallback.waitForIntercept();

            Assert.assertEquals(pageUrl, mCallback.mUrl);
            Assert.assertEquals(contentDisposition, mCallback.mContentDisposition);
            Assert.assertEquals(mimetype, mCallback.mMimetype);
            Assert.assertEquals(data.length(), mCallback.mContentLength);
            // TODO(estade): verify mUserAgent.
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Verifies that if the first navigation in a Tab is for a download then it is deleted.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(86) // Behavior landed in 86.
    public void testFirstNavigationIsDownloadClosesTab() throws Throwable {
        // Set up listening for the tab removal that we expect to happen.
        CallbackHelper onTabRemovedCallbackHelper = new CallbackHelper();
        TabListCallback tabListCallback = new TabListCallback() {
            @Override
            public void onTabRemoved(Tab tab) {
                onTabRemovedCallbackHelper.notifyCalled();
            }
        };
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { browser.registerTabListCallback(tabListCallback); });

        final String data = "download data";
        final String contentDisposition = "attachment;filename=\"download.txt\"";
        final String mimetype = "text/plain";

        List<Pair<String, String>> downloadHeaders = new ArrayList<Pair<String, String>>();
        downloadHeaders.add(Pair.create("Content-Disposition", contentDisposition));
        downloadHeaders.add(Pair.create("Content-Type", mimetype));
        downloadHeaders.add(Pair.create("Content-Length", Integer.toString(data.length())));

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse("/download.txt", data, downloadHeaders);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mActivity.getTab().getNavigationController().navigate(Uri.parse(pageUrl));
            });
            mCallback.waitForCompleted();
            onTabRemovedCallbackHelper.waitForFirst();
        } finally {
            webServer.shutdown();
        }
    }

    /**
     * Verifies the DownloadCallback is informed of downloads resulting from the user clicking on a
     * download link.
     */
    @Test
    @SmallTest
    public void testInterceptDownloadByLinkAttribute() {
        mCallback.mIntercept = true;
        String pageUrl = mActivityTestRule.getTestDataURL("download.html");
        mActivityTestRule.navigateAndWait(pageUrl);

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mCallback.waitForIntercept();
        Assert.assertEquals(mActivityTestRule.getTestDataURL("lorem_ipsum.txt"), mCallback.mUrl);
    }

    @Test
    @SmallTest
    public void testBasic() {
        String url = mActivityTestRule.getTestDataURL("content-disposition.html");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().getNavigationController().navigate(Uri.parse(url)); });
        mCallback.waitForStarted();
        mCallback.waitForCompleted();

        Assert.assertTrue(mCallback.mLocation.contains(
                "org.chromium.weblayer.shell/cache/weblayer/Downloads/"));
        if (sIsFileNameSupported) {
            Assert.assertTrue(mCallback.mFileName.contains("test"));
        }
        Assert.assertEquals(DownloadState.COMPLETE, mCallback.mState);
        Assert.assertEquals(DownloadError.NO_ERROR, mCallback.mError);
        Assert.assertEquals("text/html", mCallback.mMimetype);
    }
}

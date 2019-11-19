// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.DownloadCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests that the DownloadCallback method is invoked for downloads.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DownloadCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private Callback mCallback;

    private static class Callback extends DownloadCallback {
        public String mUrl;
        public String mUserAgent;
        public String mContentDisposition;
        public String mMimetype;
        public long mContentLength;

        @Override
        public void onDownloadRequested(String url, String userAgent, String contentDisposition,
                String mimetype, long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimetype = mimetype;
            mContentLength = contentLength;
        }

        public void waitForDownload() {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return mUrl != null;
                }
            }, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
    }

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchShellWithUrl(null);
        Assert.assertNotNull(mActivity);

        mCallback = new Callback();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setDownloadCallback(mCallback); });
    }

    /**
     * Verifies the DownloadCallback is informed of downloads resulting from navigations to pages
     * with Content-Disposition attachment.
     */
    @Test
    @SmallTest
    public void testDownloadByContentDisposition() throws Throwable {
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
            mCallback.waitForDownload();

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
     * Verifies the DownloadCallback is informed of downloads resulting from the user clicking on a
     * download link.
     */
    @Test
    @SmallTest
    public void testDownloadByLinkAttribute() {
        String pageUrl = mActivityTestRule.getTestDataURL("download.html");
        mActivityTestRule.navigateAndWait(pageUrl);

        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        mCallback.waitForDownload();
        Assert.assertEquals(mActivityTestRule.getTestDataURL("lorem_ipsum.txt"), mCallback.mUrl);
    }
}

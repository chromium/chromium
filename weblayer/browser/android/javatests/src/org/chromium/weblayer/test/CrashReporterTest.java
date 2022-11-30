// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.CrashReporterCallback;
import org.chromium.weblayer.CrashReporterController;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;

/**
 * Tests for crash reporting in WebLayer.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class CrashReporterTest {
    private static final String UUID = "032b90a6-836c-49bc-a9f4-aa210458eaf3";
    private static final String LOCAL_ID = "aa210458eaf3";
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    private File mCrashReport;
    private File mCrashSidecar;

    @Before
    public void setUp() throws IOException {
        File cacheDir =
                InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir();
        File crashReportDir = new File(cacheDir, "weblayer/Crash Reports");
        crashReportDir.mkdirs();
        mCrashReport = new File(crashReportDir, UUID + ".dmp0.try0");
        mCrashSidecar = new File(crashReportDir, UUID + ".json");
        mCrashReport.createNewFile();
        try (FileOutputStream out = new FileOutputStream(mCrashSidecar)) {
            out.write("{\"foo\":\"bar\"}".getBytes());
        }
    }

    @After
    public void tearDown() throws IOException {
        if (mCrashReport.exists()) mCrashReport.delete();
        if (mCrashSidecar.exists()) mCrashSidecar.delete();
    }

    private static final class BundleCallbackHelper extends CallbackHelper {
        private Bundle mResult;

        public Bundle getResult() {
            return mResult;
        }

        public void notifyCalled(Bundle result) {
            mResult = result;
            notifyCalled();
        }
    }

    @Test
    @SmallTest
    public void testCrashReporterLoading() throws Exception {
        BundleCallbackHelper callbackHelper = new BundleCallbackHelper();
        CallbackHelper deleteHelper = new CallbackHelper();

        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_CREATE_WEBLAYER, false);
        InstrumentationActivity activity = mActivityTestRule.launchShell(extras);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CrashReporterController crashReporterController =
                    CrashReporterController.getInstance(activity);
            // Set up a callback object that will fetch the crash keys for the crash with id
            // LOCAL_ID and then delete it.
            crashReporterController.registerCallback(new CrashReporterCallback() {
                @Override
                public void onPendingCrashReports(String[] localIds) {
                    if (!Arrays.asList(localIds).contains(LOCAL_ID)) {
                        callbackHelper.notifyFailed("localIds does not contain " + LOCAL_ID);
                        return;
                    }
                    Bundle crashKeys = crashReporterController.getCrashKeys(localIds[0]);
                    callbackHelper.notifyCalled(crashKeys);
                    crashReporterController.deleteCrash(localIds[0]);
                }

                @Override
                public void onCrashDeleted(String localId) {
                    deleteHelper.notifyCalled();
                }
            });

            // Check for crash reports ready to upload
            crashReporterController.checkForPendingCrashReports();
        });
        // Expect that a Bundle containing { "foo": "bar" } is returned.
        callbackHelper.waitForFirst();
        Bundle crashKeys = callbackHelper.getResult();
        Assert.assertArrayEquals(crashKeys.keySet().toArray(new String[0]), new String[] {"foo"});
        Assert.assertEquals(crashKeys.getString("foo"), "bar");

        // Expect that the crash report and its sidecar are deleted.
        deleteHelper.waitForFirst();
        Assert.assertFalse(mCrashReport.exists());
        Assert.assertFalse(mCrashSidecar.exists());
    }

    @MinWebLayerVersion(88) // Fix first appeared in 88.
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1355817")
    public void testBogusCrashId() throws Exception {
        CallbackHelper callbackHelper = new CallbackHelper();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CrashReporterController crashReporterController =
                    CrashReporterController.getInstance(activity);
            crashReporterController.registerCallback(new CrashReporterCallback() {
                @Override
                public void onCrashUploadFailed(String localId, String message) {
                    callbackHelper.notifyCalled();
                }
            });
            crashReporterController.uploadCrash("bogus-crash-id");
        });
        callbackHelper.waitForFirst();
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.File;
import java.io.IOException;

/**
 * Tests that WebLayer version changes handle data correctly.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class DowngradeTest {
    public static final String PREF_LAST_VERSION_CODE =
            "org.chromium.weblayer.last_version_code_used";

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // A test file in the app's data directory. This should never get deleted.
    private File mAppFile;
    // A test file in WebLayer's data directory. This should get deleted when we downgrade.
    private File mWebLayerDataFile;

    @Before
    public void setUp() throws IOException, PackageManager.NameNotFoundException {
        PathUtils.setPrivateDataDirectorySuffix("weblayer", "weblayer");
        mWebLayerDataFile = new File(PathUtils.getDataDirectory(), "testWebLayerFile");
        assertTrue(mWebLayerDataFile.createNewFile());

        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        PackageInfo packageInfo = packageManager.getPackageInfo(context.getPackageName(), 0);
        mAppFile = new File(packageInfo.applicationInfo.dataDir, "testAppFile");
        assertTrue(mAppFile.createNewFile());
    }

    @After
    public void tearDown() {
        mAppFile.delete();
        mWebLayerDataFile.delete();
    }

    @Test
    @SmallTest
    public void testDowngradeDeletesData() throws IOException {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().putInt(PREF_LAST_VERSION_CODE, 9999_000_00).apply();

        InstrumentationActivity activity = mActivityTestRule.launchWithProfile("profile");
        runOnUiThreadBlocking(
                () -> { activity.loadWebLayerSync(ContextUtils.getApplicationContext()); });

        assertFalse(mWebLayerDataFile.exists());
        assertTrue(mAppFile.exists());
    }

    @Test
    @SmallTest
    public void testUnknownLastVersionKeepsData() throws IOException {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        assertFalse(prefs.contains(PREF_LAST_VERSION_CODE));

        InstrumentationActivity activity = mActivityTestRule.launchWithProfile("profile");
        runOnUiThreadBlocking(
                () -> { activity.loadWebLayerSync(ContextUtils.getApplicationContext()); });

        assertTrue(mWebLayerDataFile.exists());
        assertTrue(mAppFile.exists());
    }

    @Test
    @SmallTest
    public void testNewVersionKeepsData() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().putInt(PREF_LAST_VERSION_CODE, 1_000_00).apply();

        InstrumentationActivity activity = mActivityTestRule.launchWithProfile("profile");
        runOnUiThreadBlocking(
                () -> { activity.loadWebLayerSync(ContextUtils.getApplicationContext()); });

        assertTrue(mWebLayerDataFile.exists());
        assertTrue(mAppFile.exists());
    }
}

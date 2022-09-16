// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.RequiresApi;
import androidx.core.app.ActivityCompat;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.ApplicationContextWrapper;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that Geolocation Web API works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class GeolocationTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private TestWebLayer mTestWebLayer;
    private TestWebServer mTestServer;
    private int mLocationPermission = PackageManager.PERMISSION_GRANTED;

    private static final String RAW_JAVASCRIPT = "var positionCount = 0;"
            + "var errorCount = 0;"
            + "function gotPos(position) {"
            + "  positionCount++;"
            + "}"
            + "function errorCallback(error){"
            + "  errorCount++;"
            + "}"
            + "function initiate_getCurrentPosition() {"
            + "  navigator.geolocation.getCurrentPosition("
            + "      gotPos, errorCallback, {});"
            + "}"
            + "function initiate_watchPosition() {"
            + "  navigator.geolocation.watchPosition("
            + "      gotPos, errorCallback, {});"
            + "}";

    @RequiresApi(Build.VERSION_CODES.M)
    private class PermissionCompatDelegate implements ActivityCompat.PermissionCompatDelegate {
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public boolean requestPermissions(
                Activity activity, String[] permissions, int requestCode) {
            mCallbackHelper.notifyCalled();
            return false;
        }

        @Override
        public boolean onActivityResult(
                Activity activity, int requestCode, int resultCode, Intent data) {
            return false;
        }

        public void waitForPermissionsRequest() throws Exception {
            mCallbackHelper.waitForFirst();
        }
    }

    @Before
    public void setUp() throws Throwable {
        Context appContext = new ApplicationContextWrapper(ContextUtils.getApplicationContext()) {
            @Override
            public int checkPermission(String permission, int pid, int uid) {
                if (permission.equals(Manifest.permission.ACCESS_FINE_LOCATION)
                        || permission.equals(Manifest.permission.ACCESS_COARSE_LOCATION)) {
                    return mLocationPermission;
                }
                return super.checkPermission(permission, pid, uid);
            }
        };
        ContextUtils.initApplicationContextForTests(appContext);

        Bundle extras = new Bundle();
        // We need to override the context with which to create WebLayer.
        extras.putBoolean(InstrumentationActivity.EXTRA_CREATE_WEBLAYER, false);
        mActivity = mActivityTestRule.launchShell(extras);
        Assert.assertNotNull(mActivity);
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.loadWebLayerSync(appContext));
        mActivityTestRule.navigateAndWait("about:blank");

        mTestWebLayer = TestWebLayer.getTestWebLayer(appContext);
        mTestWebLayer.setSystemLocationSettingEnabled(true);
        mTestWebLayer.setMockLocationProvider(true /* enable */);

        mTestServer = TestWebServer.start();

        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("geolocation.html"));
        ensureGeolocationIsRunning(false);
    }

    @After
    public void tearDown() throws Throwable {
        mTestWebLayer.setMockLocationProvider(false /* enable */);
        ensureGeolocationIsRunning(false);
    }

    /**
     * Test for navigator.getCurrentPosition.
     */
    @Test
    @MediumTest
    public void testGeolocation_getPosition() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_getCurrentPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);
        waitForCountEqual("positionCount", 1);
        mActivityTestRule.executeScriptSync("initiate_getCurrentPosition();", false);
        waitForCountEqual("positionCount", 2);
        Assert.assertEquals(0, getCountFromJS("errorCount"));
    }

    /**
     * Test for navigator.watchPosition, should receive more than one position.
     */
    @Test
    @MediumTest
    public void testGeolocation_watchPosition() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);
        waitForCountGreaterThan("positionCount", 1);
        ensureGeolocationIsRunning(true);
        Assert.assertEquals(0, getCountFromJS("errorCount"));
    }

    /**
     * Test that destroying a tab stops geolocation provider.
     */
    @Test
    @MediumTest
    public void testGeolocation_destroyTabStopsGeolocation() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);
        ensureGeolocationIsRunning(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Browser browser = mActivity.getBrowser();
            browser.destroyTab(mActivity.getBrowser().getActiveTab());
            Assert.assertEquals(0, browser.getTabs().size());
        });
        ensureGeolocationIsRunning(false);
    }

    /**
     * Test geolocation denied on insecure origins (e.g. javascript).
     */
    @Test
    @MediumTest
    public void testGeolocation_denyOnInsecureOrigins() throws Throwable {
        mActivityTestRule.navigateAndWait("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().getNavigationController().navigate(
                    Uri.parse("javascript:" + RAW_JAVASCRIPT + "initiate_getCurrentPosition();"));
        });
        waitForCountEqual("errorCount", 1);
        Assert.assertEquals(0, getCountFromJS("positionCount"));
    }

    @Test
    @MediumTest
    public void testGeolocation_denyFromPrompt() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(false);
        waitForCountEqual("errorCount", 1);
        Assert.assertEquals(0, getCountFromJS("positionCount"));
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testRequestSystemPermission() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);

        // Reload and deny the system permission, so it is prompted on the next call to geolocation.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("geolocation.html"));

        PermissionCompatDelegate delegate = new PermissionCompatDelegate();
        ActivityCompat.setPermissionCompatDelegate(delegate);
        mLocationPermission = PackageManager.PERMISSION_DENIED;
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);

        delegate.waitForPermissionsRequest();
    }

    // helper methods

    private void waitForCountEqual(String variableName, int count) {
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(getCountFromJS(variableName), Matchers.is(count)));
    }

    private void waitForDialog() throws Exception {
        // Make sure the current permission state is "prompt" before waiting for the dialog.
        mActivityTestRule.executeScriptSync("var queryResult;"
                        + "navigator.permissions.query({name: 'geolocation'}).then("
                        + "function(result) { queryResult = result.state; })",
                false);
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                String result =
                        mActivityTestRule.executeScriptSync("queryResult || ''", false)
                                .getString(InstrumentationActivityTestRule.SCRIPT_RESULT_KEY);
                Criteria.checkThat(result, Matchers.not(""));
            } catch (JSONException ex) {
                throw new CriteriaNotSatisfiedException(ex);
            }
        });
        Assert.assertEquals("prompt",
                mActivityTestRule.executeScriptSync("queryResult", false)
                        .getString(InstrumentationActivityTestRule.SCRIPT_RESULT_KEY));
        CriteriaHelper.pollInstrumentationThread(
                () -> { return mTestWebLayer.isPermissionDialogShown(); });
    }

    private void waitForCountGreaterThan(String variableName, int count) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getCountFromJS(variableName), Matchers.greaterThan(count));
        });
    }

    private void ensureGeolocationIsRunning(boolean running) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                Criteria.checkThat(
                        mTestWebLayer.isMockLocationProviderRunning(), Matchers.is(running));
            } catch (RemoteException ex) {
                throw new CriteriaNotSatisfiedException(ex);
            }
        });
    }

    private int getCountFromJS(String variableName) {
        int result = -1;
        try {
            result = mActivityTestRule.executeScriptSync(variableName, false)
                             .getInt(InstrumentationActivityTestRule.SCRIPT_RESULT_KEY);
        } catch (Exception e) {
            Assert.fail("Unable to get " + variableName);
        }
        return result;
    }
}

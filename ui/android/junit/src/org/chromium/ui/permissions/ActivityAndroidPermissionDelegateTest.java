// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Process;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowActivity.PermissionsRequest;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.UiAndroidFeatures;

import java.lang.ref.WeakReference;

/**
 * Robolectric unit tests for {@link ActivityAndroidPermissionDelegate} and {@link
 * AndroidPermissionDelegateWithRequester}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ActivityAndroidPermissionDelegateTest {
    /**
     * Rule managing the lifecycle of activity in each {@code @Test}.
     *
     * <p>To access the activity and run code on its main thread, use {@link
     * ActivityScenario#onActivity}:
     *
     * <pre>
     * mActivityScenarios.getScenario().onActivity(activity -> {
     *     // Your test code using the activity here.
     * });
     * </pre>
     */
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Test
    public void testHasPermissionDenied() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));

                            boolean hasPermission =
                                    permissionDelegate.hasPermission(
                                            android.Manifest.permission.INTERNET);

                            assertFalse(
                                    "The default result of hasPermission should be false",
                                    hasPermission);
                        });
    }

    @Test
    public void testHasPermissionGranted() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            Shadows.shadowOf(activity)
                                    .grantPermissions(android.Manifest.permission.INTERNET);
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));

                            boolean hasPermission =
                                    permissionDelegate.hasPermission(
                                            android.Manifest.permission.INTERNET);

                            assertTrue(
                                    "hasPermission should return true if permission is granted",
                                    hasPermission);
                        });
    }

    @Test
    public void testCanRequestPermissionInitial() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));

                            boolean canRequest =
                                    permissionDelegate.canRequestPermission(
                                            android.Manifest.permission.INTERNET);

                            assertTrue(
                                    "The default result of canRequestPermission should be true",
                                    canRequest);
                        });
    }

    @Test
    public void testRequestPermissionsGranted() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            PermissionCallback callback = Mockito.mock(PermissionCallback.class);

                            // Request permission
                            permissionDelegate.requestPermissions(
                                    new String[] {android.Manifest.permission.INTERNET}, callback);

                            PermissionsRequest lastRequest =
                                    Shadows.shadowOf(activity).getLastRequestedPermission();
                            assertEquals(
                                    new String[] {android.Manifest.permission.INTERNET},
                                    lastRequest.requestedPermissions);
                            String desc =
                                    "PermissionCallback should not receive results before"
                                            + " handlePermissionResult is invoked";
                            verify(callback, never().description(desc))
                                    .onRequestPermissionsResult(any(), any());

                            // Respond to the request
                            int[] grantResults = new int[] {PackageManager.PERMISSION_GRANTED};
                            permissionDelegate.handlePermissionResult(
                                    lastRequest.requestCode,
                                    lastRequest.requestedPermissions,
                                    grantResults);

                            desc = "handlePermissionResult should invoke the PermissionCallback";
                            verify(callback, Mockito.description(desc))
                                    .onRequestPermissionsResult(
                                            lastRequest.requestedPermissions, grantResults);
                        });
    }

    @Test
    public void testRequestPermissionsDenied() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            PermissionCallback callback = Mockito.mock(PermissionCallback.class);

                            // Request permission
                            permissionDelegate.requestPermissions(
                                    new String[] {android.Manifest.permission.INTERNET}, callback);

                            PermissionsRequest lastRequest =
                                    Shadows.shadowOf(activity).getLastRequestedPermission();
                            assertEquals(
                                    new String[] {android.Manifest.permission.INTERNET},
                                    lastRequest.requestedPermissions);
                            String desc =
                                    "PermissionCallback should not receive results before"
                                            + " handlePermissionResult is invoked";
                            verify(callback, never().description(desc))
                                    .onRequestPermissionsResult(any(), any());

                            // Respond to the request
                            int[] grantResults = new int[] {PackageManager.PERMISSION_DENIED};
                            permissionDelegate.handlePermissionResult(
                                    lastRequest.requestCode,
                                    lastRequest.requestedPermissions,
                                    grantResults);

                            desc = "handlePermissionResult should invoke the PermissionCallback";
                            verify(callback, Mockito.description(desc))
                                    .onRequestPermissionsResult(
                                            lastRequest.requestedPermissions, grantResults);
                        });
    }

    @Test
    public void testCanRequestPermissionAfterRequestGranted() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            performRequestPermission(
                                    permissionDelegate,
                                    Shadows.shadowOf(activity),
                                    android.Manifest.permission.INTERNET,
                                    PackageManager.PERMISSION_GRANTED);

                            boolean canRequest =
                                    permissionDelegate.canRequestPermission(
                                            android.Manifest.permission.INTERNET);

                            assertTrue(
                                    "After a granted permission request canRequestPermission should"
                                            + " return true",
                                    canRequest);
                        });
    }

    @Test
    public void
            testCanRequestPermissionRequestDenied_shouldNotShowRationale_prevShouldShowRationale() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            Shadows.shadowOf(activity.getPackageManager())
                                    .setShouldShowRequestPermissionRationale(
                                            android.Manifest.permission.INTERNET, true);
                            performRequestPermission(
                                    permissionDelegate,
                                    Shadows.shadowOf(activity),
                                    android.Manifest.permission.INTERNET,
                                    PackageManager.PERMISSION_DENIED);
                            Shadows.shadowOf(activity.getPackageManager())
                                    .setShouldShowRequestPermissionRationale(
                                            android.Manifest.permission.INTERNET, false);
                            boolean canRequest =
                                    permissionDelegate.canRequestPermission(
                                            android.Manifest.permission.INTERNET);

                            assertFalse(
                                    "After a denied permission request canRequestPermission should"
                                        + " return false if shouldShowRequestPermissionRationale"
                                        + " returns false after previously returning true",
                                    canRequest);
                        });
    }

    @Test
    public void
            testCanRequestPermissionRequestDenied_shouldNotShowRationale_prevShouldNotShowRationale() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            Shadows.shadowOf(activity.getPackageManager())
                                    .setShouldShowRequestPermissionRationale(
                                            android.Manifest.permission.INTERNET, false);
                            performRequestPermission(
                                    permissionDelegate,
                                    Shadows.shadowOf(activity),
                                    android.Manifest.permission.INTERNET,
                                    PackageManager.PERMISSION_DENIED);
                            Shadows.shadowOf(activity.getPackageManager())
                                    .setShouldShowRequestPermissionRationale(
                                            android.Manifest.permission.INTERNET, false);
                            boolean canRequest =
                                    permissionDelegate.canRequestPermission(
                                            android.Manifest.permission.INTERNET);

                            if (Build.VERSION.SDK_INT < 30 /*Build.VERSION_CODES.R*/) {
                                assertFalse(
                                        "After a denied permission request canRequestPermission"
                                            + " should return false if"
                                            + " shouldShowRequestPermissionRationale returns false",
                                        canRequest);
                            } else {
                                // This can happen in Android.R>= when a user dismissed permission
                                // dialog before taking any action.
                                assertTrue(
                                        "After a denied permission request canRequestPermission"
                                            + " should return true if"
                                            + " shouldShowRequestPermissionRationale returns false"
                                            + " after previously returning false",
                                        canRequest);
                            }
                        });
    }

    @Test
    public void testCanRequestPermissionWithShowRequestRationale() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            performRequestPermission(
                                    permissionDelegate,
                                    Shadows.shadowOf(activity),
                                    android.Manifest.permission.INTERNET,
                                    PackageManager.PERMISSION_DENIED);
                            Shadows.shadowOf(activity.getPackageManager())
                                    .setShouldShowRequestPermissionRationale(
                                            android.Manifest.permission.INTERNET, true);

                            boolean canRequest =
                                    permissionDelegate.canRequestPermission(
                                            android.Manifest.permission.INTERNET);

                            assertTrue(
                                    "When shouldShowRequestPermissionRationale is true "
                                            + "canRequestPermission should return true",
                                    canRequest);
                        });
    }

    @Test
    public void testCanRequestPermissionAfterHasPermissionGranted() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            performRequestPermission(
                                    permissionDelegate,
                                    Shadows.shadowOf(activity),
                                    android.Manifest.permission.INTERNET,
                                    PackageManager.PERMISSION_DENIED);

                            Shadows.shadowOf(activity)
                                    .grantPermissions(android.Manifest.permission.INTERNET);
                            permissionDelegate.hasPermission(android.Manifest.permission.INTERNET);
                            Shadows.shadowOf(activity)
                                    .denyPermissions(android.Manifest.permission.INTERNET);

                            boolean canRequest =
                                    permissionDelegate.canRequestPermission(
                                            android.Manifest.permission.INTERNET);

                            assertTrue(
                                    "After hasPermission sees that a permission is granted "
                                            + "canRequestPermission should return true",
                                    canRequest);
                        });
    }

    @Test
    public void testCanRequestPermissionWhileGranted() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            AndroidPermissionDelegate permissionDelegate =
                                    new ActivityAndroidPermissionDelegate(
                                            new WeakReference<>(activity));
                            performRequestPermission(
                                    permissionDelegate,
                                    Shadows.shadowOf(activity),
                                    android.Manifest.permission.INTERNET,
                                    PackageManager.PERMISSION_DENIED);
                            Shadows.shadowOf(activity)
                                    .grantPermissions(android.Manifest.permission.INTERNET);

                            boolean canRequest =
                                    permissionDelegate.canRequestPermission(
                                            android.Manifest.permission.INTERNET);

                            assertTrue(
                                    "If a permission is currently granted canRequestPermission"
                                            + " should return true",
                                    canRequest);
                        });
    }

    private static final String ACCESS_LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK";

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    @EnableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void testLocalNetworkPermission_revokedCompat_targetSdkBelow37() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            Context context = ContextUtils.getApplicationContext();
                            int prevTargetSdk = context.getApplicationInfo().targetSdkVersion;
                            try {
                                context.getApplicationInfo().targetSdkVersion = 36;
                                // In REVOKED_COMPAT, PackageManager reports PERMISSION_GRANTED
                                // while AppOpsManager reports MODE_IGNORED.
                                Shadows.shadowOf(activity).grantPermissions(ACCESS_LOCAL_NETWORK);
                                AppOpsManager appOpsManager =
                                        (AppOpsManager)
                                                context.getSystemService(Context.APP_OPS_SERVICE);
                                String op = AppOpsManager.permissionToOp(ACCESS_LOCAL_NETWORK);
                                assertNotNull(op);
                                Shadows.shadowOf(appOpsManager)
                                        .setMode(
                                                op,
                                                Process.myUid(),
                                                context.getPackageName(),
                                                AppOpsManager.MODE_IGNORED);

                                AndroidPermissionDelegate permissionDelegate =
                                        new ActivityAndroidPermissionDelegate(
                                                new WeakReference<>(activity));

                                assertFalse(
                                        "hasPermission should return false when AppOps is"
                                                + " MODE_IGNORED (REVOKED_COMPAT)",
                                        permissionDelegate.hasPermission(ACCESS_LOCAL_NETWORK));
                                assertFalse(
                                        "canRequestPermission should return false when"
                                                + " targetSdkVersion < 37 and permission is"
                                                + " compat-revoked",
                                        permissionDelegate.canRequestPermission(
                                                ACCESS_LOCAL_NETWORK));

                                // Once the user re-enables the permission in Android Settings,
                                // AppOps becomes MODE_ALLOWED.
                                Shadows.shadowOf(appOpsManager)
                                        .setMode(
                                                op,
                                                Process.myUid(),
                                                context.getPackageName(),
                                                AppOpsManager.MODE_ALLOWED);
                                assertTrue(
                                        "hasPermission should return true when AppOps is"
                                                + " MODE_ALLOWED",
                                        permissionDelegate.hasPermission(ACCESS_LOCAL_NETWORK));
                                assertTrue(
                                        "canRequestPermission should return true when permission"
                                                + " is granted",
                                        permissionDelegate.canRequestPermission(
                                                ACCESS_LOCAL_NETWORK));
                            } finally {
                                context.getApplicationInfo().targetSdkVersion = prevTargetSdk;
                            }
                        });
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    @EnableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void testLocalNetworkPermission_targetSdk37() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            Context context = ContextUtils.getApplicationContext();
                            int prevTargetSdk = context.getApplicationInfo().targetSdkVersion;
                            try {
                                context.getApplicationInfo().targetSdkVersion = 37;
                                AndroidPermissionDelegate permissionDelegate =
                                        new ActivityAndroidPermissionDelegate(
                                                new WeakReference<>(activity));

                                assertFalse(permissionDelegate.hasPermission(ACCESS_LOCAL_NETWORK));
                                assertTrue(
                                        "When targeting SDK 37, ACCESS_LOCAL_NETWORK can be"
                                                + " requested via runtime dialog",
                                        permissionDelegate.canRequestPermission(
                                                ACCESS_LOCAL_NETWORK));

                                Shadows.shadowOf(activity).grantPermissions(ACCESS_LOCAL_NETWORK);
                                assertTrue(permissionDelegate.hasPermission(ACCESS_LOCAL_NETWORK));
                            } finally {
                                context.getApplicationInfo().targetSdkVersion = prevTargetSdk;
                            }
                        });
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    @DisableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void testLocalNetworkPermission_revokedCompat_featureDisabled() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            Context context = ContextUtils.getApplicationContext();
                            int prevTargetSdk = context.getApplicationInfo().targetSdkVersion;
                            try {
                                context.getApplicationInfo().targetSdkVersion = 36;
                                Shadows.shadowOf(activity).grantPermissions(ACCESS_LOCAL_NETWORK);
                                AppOpsManager appOpsManager =
                                        (AppOpsManager)
                                                context.getSystemService(Context.APP_OPS_SERVICE);
                                String op = AppOpsManager.permissionToOp(ACCESS_LOCAL_NETWORK);
                                assertNotNull(op);
                                Shadows.shadowOf(appOpsManager)
                                        .setMode(
                                                op,
                                                Process.myUid(),
                                                context.getPackageName(),
                                                AppOpsManager.MODE_IGNORED);

                                AndroidPermissionDelegate permissionDelegate =
                                        new ActivityAndroidPermissionDelegate(
                                                new WeakReference<>(activity));

                                assertTrue(
                                        "When kill switch is disabled, hasPermission falls back to"
                                                + " PackageManager check",
                                        permissionDelegate.hasPermission(ACCESS_LOCAL_NETWORK));
                            } finally {
                                context.getApplicationInfo().targetSdkVersion = prevTargetSdk;
                            }
                        });
    }

    /**
     * Calls {@link AndroidPermissionDelegate#requestPermissions} and {@link
     * AndroidPermissionDelegate#handlePermissionResult} for a single permission.
     */
    private void performRequestPermission(
            AndroidPermissionDelegate permissionDelegate,
            ShadowActivity shadowActivity,
            String permission,
            int grantResult) {
        permissionDelegate.requestPermissions(
                new String[] {permission}, Mockito.mock(PermissionCallback.class));
        PermissionsRequest lastRequest = shadowActivity.getLastRequestedPermission();
        permissionDelegate.handlePermissionResult(
                lastRequest.requestCode, lastRequest.requestedPermissions, new int[] {grantResult});
    }
}

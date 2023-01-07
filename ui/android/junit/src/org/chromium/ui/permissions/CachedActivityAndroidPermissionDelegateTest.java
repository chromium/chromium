// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.Manifest.permission;
import android.content.pm.PackageManager;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowActivity.PermissionsRequest;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

import java.lang.ref.WeakReference;

/**
 * Robolectric unit tests for {@link CachedActivityAndroidPermissionDelegate}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedActivityAndroidPermissionDelegateTest {
    static final String TEST_PERMISSION = permission.INTERNET;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Test
    public void testHasPermissionWithCache() {
        testHasPermission(true);
    }

    @Test
    public void testHasPermissionWithoutCache() {
        testHasPermission(false);
    }

    @Test
    public void testCanRequestPermissionWithCache() {
        testCanRequestPermission(true);
    }

    @Test
    public void testCanRequestPermissionWithoutCache() {
        testCanRequestPermission(false);
    }

    @Test
    public void testInvalidateCache() {
        mActivityScenarios.getScenario().onActivity(activity -> {
            CachedActivityAndroidPermissionDelegate permissionDelegate =
                    new CachedActivityAndroidPermissionDelegate(new WeakReference(activity));

            Shadows.shadowOf(activity).grantPermissions(TEST_PERMISSION);

            boolean hasPermission = permissionDelegate.hasPermission(TEST_PERMISSION);
            boolean canRequestPermission = permissionDelegate.canRequestPermission(TEST_PERMISSION);
            assertTrue("The first call to hasPermission should be true", hasPermission);
            assertTrue(
                    "The first call to canRequestPermission should be true", canRequestPermission);

            denyPermissionRequest(permissionDelegate, Shadows.shadowOf(activity));
            Shadows.shadowOf(activity).denyPermissions(TEST_PERMISSION);
            permissionDelegate.invalidateCache();

            // we just invalidated the cache, so we expect both values to be false.
            hasPermission = permissionDelegate.hasPermission(TEST_PERMISSION);
            canRequestPermission = permissionDelegate.canRequestPermission(TEST_PERMISSION);
            assertFalse(hasPermission);
            assertFalse(canRequestPermission);
        });
    }

    private void denyPermissionRequest(
            CachedActivityAndroidPermissionDelegate permissionDelegate, ShadowActivity activity) {
        permissionDelegate.requestPermissions(
                new String[] {TEST_PERMISSION}, Mockito.mock(PermissionCallback.class));
        PermissionsRequest lastRequest = activity.getLastRequestedPermission();
        permissionDelegate.handlePermissionResult(lastRequest.requestCode,
                lastRequest.requestedPermissions, new int[] {PackageManager.PERMISSION_DENIED});
    }

    private void testHasPermission(boolean cacheEnabled) {
        mActivityScenarios.getScenario().onActivity(activity -> {
            CachedActivityAndroidPermissionDelegate permissionDelegate =
                    new CachedActivityAndroidPermissionDelegate(new WeakReference(activity));
            permissionDelegate.setCacheEnabledForTesting(cacheEnabled);

            Shadows.shadowOf(activity).grantPermissions(TEST_PERMISSION);
            boolean hasPermission = permissionDelegate.hasPermission(TEST_PERMISSION);
            assertTrue("The first call to hasPermission should be true", hasPermission);

            Shadows.shadowOf(activity).denyPermissions(TEST_PERMISSION);
            hasPermission = permissionDelegate.hasPermission(TEST_PERMISSION);

            if (cacheEnabled) {
                assertTrue("Subsequent calls to hasPermission should return the cached result",
                        hasPermission);
            } else {
                assertFalse("Subsequent calls to hasPermission should not return the cached result",
                        hasPermission);
            }
        });
    }

    private void testCanRequestPermission(boolean cacheEnabled) {
        mActivityScenarios.getScenario().onActivity(activity -> {
            CachedActivityAndroidPermissionDelegate permissionDelegate =
                    new CachedActivityAndroidPermissionDelegate(new WeakReference(activity));
            permissionDelegate.setCacheEnabledForTesting(cacheEnabled);

            boolean canRequestPermission = permissionDelegate.canRequestPermission(TEST_PERMISSION);
            assertTrue(
                    "The first call to canRequestPermission should be true", canRequestPermission);

            denyPermissionRequest(permissionDelegate, Shadows.shadowOf(activity));
            canRequestPermission = permissionDelegate.canRequestPermission(TEST_PERMISSION);

            // Without caching, canRequestPermission() will return false because we denied the
            // permission request.
            if (cacheEnabled) {
                assertTrue(canRequestPermission);
            } else {
                assertFalse(canRequestPermission);
            }
        });
    }
}

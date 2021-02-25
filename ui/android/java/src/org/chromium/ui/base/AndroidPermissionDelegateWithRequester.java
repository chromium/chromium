// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PermissionInfo;
import android.os.Build;
import android.os.Handler;
import android.os.Process;
import android.text.TextUtils;
import android.util.SparseArray;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * AndroidPermissionDelegate that implements much of the logic around requesting permissions.
 * Subclasses need to implement only the basic permissions checking and requesting methods.
 */
public abstract class AndroidPermissionDelegateWithRequester implements AndroidPermissionDelegate {
    private Handler mHandler;
    private SparseArray<PermissionRequestInfo> mOutstandingPermissionRequests;
    private int mNextRequestCode;

    // Constants used for permission request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;

    /**
     * Shared preference key prefix for remembering Android permissions denied by the user.
     * <p>
     * <b>NOTE:</b> As of M86 the semantics of shared prefs using this key prefix has changed:
     * <ul>
     *   <li>Previously: {@code true} if the user was ever asked for a permission, otherwise absent.
     *   <li>M86+: {@code true} if the user most recently has denied permission access,
     *     otherwise absent.
     * </ul>
     */
    private static final String PERMISSION_WAS_DENIED_KEY_PREFIX =
            "HasRequestedAndroidPermission::";

    public AndroidPermissionDelegateWithRequester() {
        mHandler = new Handler();
        mOutstandingPermissionRequests = new SparseArray<PermissionRequestInfo>();
    }

    @Override
    public final boolean hasPermission(String permission) {
        boolean isGranted =
                ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                        permission, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
        if (isGranted) {
            clearPermissionWasDenied(permission);
        }
        return isGranted;
    }

    /**
     * Clear the shared pref indicating that {@code permission} was denied by the user.
     */
    private void clearPermissionWasDenied(String permission) {
        String key = getPermissionWasDeniedKey(permission);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        if (!prefs.contains(key)) return;

        SharedPreferences.Editor editor = prefs.edit();
        editor.remove(key);
        editor.apply();
    }

    /** @see Activity.shouldShowRequestPermissionRationale */
    protected abstract boolean shouldShowRequestPermissionRationale(String permission);

    @Override
    public final boolean canRequestPermission(String permission) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;

        if (hasPermission(permission)) {
            // There is no need to call clearPermissionWasDenied - hasPermission already cleared
            // the shared pref if needed.
            return true;
        }

        if (isPermissionRevokedByPolicy(permission)) {
            return false;
        }

        if (shouldShowRequestPermissionRationale(permission)) {
            // This information from Android suggests we should not assume the user will always deny
            // the permission.
            clearPermissionWasDenied(permission);
            return true;
        }

        // Check whether we have been denied this permission by checking whether we saved
        // a preference associated with it before.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        return !prefs.getBoolean(getPermissionWasDeniedKey(permission), false);
    }

    /** @see PackageManager#isPermissionRevokedByPolicy(String, String) */
    protected abstract boolean isPermissionRevokedByPolicyInternal(String permission);

    @Override
    public final boolean isPermissionRevokedByPolicy(String permission) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        return isPermissionRevokedByPolicyInternal(permission);
    }

    @Override
    public final void requestPermissions(
            final String[] permissions, final PermissionCallback callback) {
        if (requestPermissionsInternal(permissions, callback)) return;

        // If the permission request was not sent successfully, just post a response to the
        // callback with whatever the current permission state is for all the requested
        // permissions.  The response is posted to keep the async behavior of this method
        // consistent.
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                int[] results = new int[permissions.length];
                for (int i = 0; i < permissions.length; i++) {
                    results[i] = hasPermission(permissions[i]) ? PackageManager.PERMISSION_GRANTED
                                                               : PackageManager.PERMISSION_DENIED;
                }
                callback.onRequestPermissionsResult(permissions, results);
            }
        });
    }

    @Override
    public final boolean handlePermissionResult(
            int requestCode, String[] permissions, int[] grantResults) {
        PermissionRequestInfo requestInfo = mOutstandingPermissionRequests.get(requestCode);
        mOutstandingPermissionRequests.delete(requestCode);

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        for (int i = 0; i < permissions.length; i++) {
            if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                editor.remove(getPermissionWasDeniedKey(permissions[i]));
            } else if (shouldPersistDenial(requestInfo, permissions[i])) {
                editor.putBoolean(getPermissionWasDeniedKey(permissions[i]), true);
            }
        }
        editor.apply();

        if (requestInfo == null || requestInfo.callback == null) return false;
        requestInfo.callback.onRequestPermissionsResult(permissions, grantResults);
        return true;
    }

    /**
     * Returns if an information about permission denial should be stored. Denial should not be
     * stored iff:
     * <ul>
     *   <li> Android version >= Android.R
     *   <li> The information about initial @see Activity.shouldShowRequestPermissionRationale is
     *        present and the value is false
     *   <li> The current value of @see Activity.shouldShowRequestPermissionRationale is false as
     *        well
     * </ul>
     */
    private boolean shouldPersistDenial(PermissionRequestInfo requestInfo, String permission) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return true;

        boolean initialShowRationaleState = false;
        if (requestInfo != null) {
            initialShowRationaleState = requestInfo.getInitialShowRationaleStateFor(permission);
        }

        return initialShowRationaleState || shouldShowRequestPermissionRationale(permission);
    }

    /** @see Activity.requestPermissions */
    protected abstract boolean requestPermissionsFromRequester(
            String[] permissions, int requestCode);

    /**
     * Issues the permission request and returns whether it was sent successfully.
     */
    private boolean requestPermissionsInternal(String[] permissions, PermissionCallback callback) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;

        int requestCode = REQUEST_CODE_PREFIX + mNextRequestCode;
        mNextRequestCode = (mNextRequestCode + 1) % REQUEST_CODE_RANGE_SIZE;
        mOutstandingPermissionRequests.put(
                requestCode, new PermissionRequestInfo(permissions, callback));
        if (!requestPermissionsFromRequester(permissions, requestCode)) {
            mOutstandingPermissionRequests.delete(requestCode);
            return false;
        }
        return true;
    }

    private String normalizePermissionName(String permission) {
        // Prior to O, permissions were granted at the group level.  Post O, each permission is
        // granted individually.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            try {
                // Runtime permissions are controlled at the group level.  So when determining
                // whether we have requested a particular permission before, we should check whether
                // we have requested any permission in that group as that mimics the logic in the
                // Android framework.
                //
                // e.g. Requesting first the permission ACCESS_FINE_LOCATION will result in Chrome
                //      treating ACCESS_COARSE_LOCATION as if it had already been requested as well.
                PermissionInfo permissionInfo =
                        ContextUtils.getApplicationContext().getPackageManager().getPermissionInfo(
                                permission, PackageManager.GET_META_DATA);

                if (!TextUtils.isEmpty(permissionInfo.group)) {
                    return permissionInfo.group;
                }
            } catch (NameNotFoundException e) {
                // Unknown permission.  Default back to the permission name instead of the group.
            }
        }

        return permission;
    }

    /**
     * Returns the name of a shared preferences key used to store whether Chrome was denied
     * {@code permission}.
     */
    private String getPermissionWasDeniedKey(String permission) {
        return PERMISSION_WAS_DENIED_KEY_PREFIX + normalizePermissionName(permission);
    }

    /** Wrapper holding information relevant to a permission request. */
    private class PermissionRequestInfo {
        public final PermissionCallback callback;
        public final Map<String, Boolean> initialShowRationaleState;

        public PermissionRequestInfo(String[] permissions, PermissionCallback callback) {
            initialShowRationaleState = new HashMap<>();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                for (String permission : permissions) {
                    initialShowRationaleState.put(
                            permission, shouldShowRequestPermissionRationale(permission));
                }
            }
            this.callback = callback;
        }

        /**
         * Returns initial value of @see Activity.shouldShowRequestPermissionRationale
         * for the given {@code permission} or false if not found.
         */
        public boolean getInitialShowRationaleStateFor(String permission) {
            assert initialShowRationaleState.get(permission) != null;
            return initialShowRationaleState.get(permission) != null
                    ? initialShowRationaleState.get(permission)
                    : false;
        }
    }
}

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

/**
 * AndroidPermissionDelegate that implements much of the logic around requesting permissions.
 * Subclasses need to implement only the basic permissions checking and requesting methods.
 */
public abstract class AndroidPermissionDelegateWithRequester implements AndroidPermissionDelegate {
    private Handler mHandler;
    private SparseArray<PermissionCallback> mOutstandingPermissionRequests;
    private int mNextRequestCode;

    // Constants used for permission request code bounding.
    private static final int REQUEST_CODE_PREFIX = 1000;
    private static final int REQUEST_CODE_RANGE_SIZE = 100;

    private static final String PERMISSION_QUERIED_KEY_PREFIX = "HasRequestedAndroidPermission::";

    public AndroidPermissionDelegateWithRequester() {
        mHandler = new Handler();
        mOutstandingPermissionRequests = new SparseArray<PermissionCallback>();
    }

    @Override
    public final boolean hasPermission(String permission) {
        return ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                       permission, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /** @see Activity.shouldShowRequestPermissionRationale */
    protected abstract boolean shouldShowRequestPermissionRationale(String permission);

    @Override
    public final boolean canRequestPermission(String permission) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;

        if (isPermissionRevokedByPolicy(permission)) {
            return false;
        }

        if (shouldShowRequestPermissionRationale(permission)) {
            return true;
        }

        // Check whether we have ever asked for this permission by checking whether we saved
        // a preference associated with it before.
        String permissionQueriedKey = getHasRequestedPermissionKey(permission);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        if (!prefs.getBoolean(permissionQueriedKey, false)) return true;

        logUMAOnRequestPermissionDenied(permission);
        return false;
    }

    /** @see PackageManager.isPermissionRevokedByPolicy */
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
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        for (int i = 0; i < permissions.length; i++) {
            editor.putBoolean(getHasRequestedPermissionKey(permissions[i]), true);
        }
        editor.apply();

        PermissionCallback callback = mOutstandingPermissionRequests.get(requestCode);
        mOutstandingPermissionRequests.delete(requestCode);
        if (callback == null) return false;
        callback.onRequestPermissionsResult(permissions, grantResults);
        return true;
    }

    protected void logUMAOnRequestPermissionDenied(String permission) {}

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
        mOutstandingPermissionRequests.put(requestCode, callback);
        if (!requestPermissionsFromRequester(permissions, requestCode)) {
            mOutstandingPermissionRequests.delete(requestCode);
            return false;
        }
        return true;
    }

    private String getHasRequestedPermissionKey(String permission) {
        String permissionQueriedKey = permission;
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
                    permissionQueriedKey = permissionInfo.group;
                }
            } catch (NameNotFoundException e) {
                // Unknown permission.  Default back to the permission name instead of the group.
            }
        }

        return PERMISSION_QUERIED_KEY_PREFIX + permissionQueriedKey;
    }
}

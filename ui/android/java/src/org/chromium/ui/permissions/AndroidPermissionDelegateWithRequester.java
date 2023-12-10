// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Process;
import android.util.SparseArray;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
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

    public AndroidPermissionDelegateWithRequester() {
        mHandler = new Handler();
        mOutstandingPermissionRequests = new SparseArray<PermissionRequestInfo>();
    }

    @Override
    public final boolean hasPermission(String permission) {
        boolean isGranted =
                ApiCompatibilityUtils.checkPermission(
                                ContextUtils.getApplicationContext(),
                                permission,
                                Process.myPid(),
                                Process.myUid())
                        == PackageManager.PERMISSION_GRANTED;
        if (isGranted) {
            PermissionPrefs.clearPermissionWasDenied(permission);
        }
        return isGranted;
    }

    @Override
    public final boolean canRequestPermission(String permission) {
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
            PermissionPrefs.clearPermissionWasDenied(permission);
            return true;
        }

        // Check whether we have been denied this permission by checking whether we saved
        // a preference associated with it before. This is to identify the cases where the app never
        // requested for the permission before.
        return !PermissionPrefs.wasPermissionDenied(permission);
    }

    /** @see PackageManager#isPermissionRevokedByPolicy(String, String) */
    protected abstract boolean isPermissionRevokedByPolicyInternal(String permission);

    @Override
    public final boolean isPermissionRevokedByPolicy(String permission) {
        return isPermissionRevokedByPolicyInternal(permission);
    }

    @Override
    public final void requestPermissions(
            final String[] permissions, final PermissionCallback callback) {
        if (requestPermissionsInternal(permissions, callback)) {
            PermissionPrefs.onAndroidPermissionRequestUiShown(permissions);
            return;
        }

        // If the permission request was not sent successfully, just post a response to the
        // callback with whatever the current permission state is for all the requested
        // permissions.  The response is posted to keep the async behavior of this method
        // consistent.
        mHandler.post(
                new Runnable() {
                    @Override
                    public void run() {
                        int[] results = new int[permissions.length];
                        for (int i = 0; i < permissions.length; i++) {
                            results[i] =
                                    hasPermission(permissions[i])
                                            ? PackageManager.PERMISSION_GRANTED
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

        List<String> permissionsGranted = new ArrayList<>();
        List<String> permissionsDenied = new ArrayList<>();
        for (int i = 0; i < permissions.length; i++) {
            if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                permissionsGranted.add(permissions[i]);
            } else if (shouldPersistDenial(requestInfo, permissions[i])) {
                permissionsDenied.add(permissions[i]);
            }
        }
        PermissionPrefs.editPermissionsPref(permissionsGranted, permissionsDenied);

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

    /** Issues the permission request and returns whether it was sent successfully. */
    private boolean requestPermissionsInternal(String[] permissions, PermissionCallback callback) {
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

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

/** Contains the functionality for interacting with the android permissions system. */
public interface AndroidPermissionDelegate {
    /**
     * Determine whether access to a particular permission is granted.
     *
     * @param permission The permission whose access is to be checked.
     * @return Whether access to the permission is granted.
     */
    boolean hasPermission(String permission);

    /**
     * Determine whether the specified permission can be requested.
     * <p>
     * A permission can not be requested in the following states:
     * <ul>
     *   <li>Permission is denied by policy.
     *   <li>Permission previously denied and the user selected "Never ask again".
     * </ul>
     *
     * @param permission The permission name.
     * @return Whether the permission can be requested.
     */
    boolean canRequestPermission(String permission);

    /**
     * Determine whether the specified permission is revoked by policy.
     *
     * @param permission The permission name.
     * @return Whether the permission is revoked by policy and the user has no ability to change it.
     */
    boolean isPermissionRevokedByPolicy(String permission);

    /**
     * Requests the specified permissions are granted for further use.
     *
     * @param permissions The list of permissions to request access to.
     * @param callback The callback to be notified whether the permissions were granted.
     */
    void requestPermissions(String[] permissions, PermissionCallback callback);

    /**
     * Handle the result from requesting permissions.
     *
     * @param requestCode The request code passed in requestPermissions.
     * @param permissions The list of requested permissions.
     * @param grantResults The grant results for the corresponding permissions which is either
     *         {@link android.content.pm.PackageManager#PERMISSION_GRANTED} or {@link
     *         android.content.pm.PackageManager#PERMISSION_DENIED}.
     * @return True if the result was handled.
     */
    boolean handlePermissionResult(int requestCode, String[] permissions, int[] grantResults);

    /**
     * Called to determine whether android suggests showing a promo rationale.
     * @see {@link Activity#shouldShowRequestPermissionRationale(String)}.
     */
    default boolean shouldShowRequestPermissionRationale(String permission) {
        return false;
    }
}

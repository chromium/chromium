// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import android.Manifest;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PermissionInfo;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;

import java.util.List;

/**
 * Provides helper methods for shared preference access to store permission request related info.
 */
public class PermissionPrefs {
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

    /**
     * Shared preference key prefix for storing the timestamp of when the permission request was
     * shown. Only used for notification permission currently.
     */
    private static final String ANDROID_PERMISSION_REQUEST_TIMESTAMP_KEY_PREFIX =
            "AndroidPermissionRequestTimestamp::";

    /**
     * Returns normalized permission name for the given permission considering OS versions.
     * @param permission The permission name.
     * @return Normalized permission name.
     */
    public static String normalizePermissionName(String permission) {
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
                        ContextUtils.getApplicationContext()
                                .getPackageManager()
                                .getPermissionInfo(permission, PackageManager.GET_META_DATA);

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
     * NOTE: Use this method with caution. The pref is aggressively cleared on
     * permission grant, or on shouldShowRequestPermissionRationale returning true.
     * @return Whether the request was denied by the user for the given {@code permission}
     */
    static boolean wasPermissionDenied(String permission) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        return prefs.getBoolean(getPermissionWasDeniedKey(permission), false);
    }

    /** Clear the shared pref indicating that {@code permission} was denied by the user. */
    static void clearPermissionWasDenied(String permission) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.remove(getPermissionWasDeniedKey(permission));
        editor.apply();
    }

    /**
     * Saves/deletes the given list of permissions from shared prefs. Permission entries are deleted
     * on permission grant and added on denial.
     * @param permissionsGranted The list of permissions to delete entries.
     * @param permissionsDenied The list of permissions to add/update entries.
     */
    static void editPermissionsPref(
            List<String> permissionsGranted, List<String> permissionsDenied) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        for (String permission : permissionsGranted) {
            editor.remove(getPermissionWasDeniedKey(permission));
        }
        for (String permission : permissionsDenied) {
            editor.putBoolean(getPermissionWasDeniedKey(permission), true);
        }
        editor.apply();
    }

    /**
     * @return The timestamp when the notification permission request was shown last.
     */
    public static long getAndroidNotificationPermissionRequestTimestamp() {
        String prefName =
                ANDROID_PERMISSION_REQUEST_TIMESTAMP_KEY_PREFIX
                        + PermissionPrefs.normalizePermissionName(
                                Manifest.permission.POST_NOTIFICATIONS);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        return prefs.getLong(prefName, 0);
    }

    /** Called when the android permission prompt was shown. */
    static void onAndroidPermissionRequestUiShown(String[] permissions) {
        boolean isNotification = false;
        for (String permission : permissions) {
            if (TextUtils.equals(permission, Manifest.permission.POST_NOTIFICATIONS)) {
                isNotification = true;
                break;
            }
        }
        if (!isNotification) return;

        String prefName =
                ANDROID_PERMISSION_REQUEST_TIMESTAMP_KEY_PREFIX
                        + PermissionPrefs.normalizePermissionName(
                                Manifest.permission.POST_NOTIFICATIONS);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().putLong(prefName, TimeUtils.currentTimeMillis()).apply();
    }

    /**
     * Returns the name of a shared preferences key used to store whether Chrome was denied
     * {@code permission}.
     */
    private static String getPermissionWasDeniedKey(String permission) {
        return PERMISSION_WAS_DENIED_KEY_PREFIX + normalizePermissionName(permission);
    }
}

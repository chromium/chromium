// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

/**
 * Utility class with constants related to permissions.
 */
public final class PermissionConstants {
    /** The permission string for notification permission. */
    // TODO(shaktisahu): Replace this with permission constant from {@link Manifest.permission}.
    public static final String NOTIFICATION_PERMISSION = "android.permission.POST_NOTIFICATIONS";

    // TODO(finnur): Replace this with permission constant from {@link Manifest.permission}.
    public static final String READ_MEDIA_AUDIO = "android.permission.READ_MEDIA_AUDIO";
    public static final String READ_MEDIA_IMAGES = "android.permission.READ_MEDIA_IMAGES";
    public static final String READ_MEDIA_VIDEO = "android.permission.READ_MEDIA_VIDEO";
}

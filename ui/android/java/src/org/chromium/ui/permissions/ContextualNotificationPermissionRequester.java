// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import androidx.annotation.Nullable;

/**
 * Contains functionality to request Android notification permission contextually. This will result
 * in showing the Android permission prompt without any rationale. This is a singleton class with
 * the implementation provided from chrome layer. Internally delegates the permission request to
 * {@link NotificationPermissionController}.
 */
public abstract class ContextualNotificationPermissionRequester {
    private static ContextualNotificationPermissionRequester sInstance;

    /** @return The singleton instance of this class. */
    @Nullable
    public static ContextualNotificationPermissionRequester getInstance() {
        return sInstance;
    }

    /**
     * Called to set the singleton instance.
     * @param instance An instance of {@link ContextualNotificationPermissionRequester}.
     */
    public static void setInstance(ContextualNotificationPermissionRequester instance) {
        sInstance = instance;
    }

    /**
     * Called to request notification permission if needed. This will result in showing the Android
     * permission prompt for notifications.
     */
    public abstract void requestPermissionIfNeeded();

    /**
     * Called to determine whether or not we can request site notification permission from app level
     * settings perspective. We shouldn't request site permission if
     * (1) For Android versions < T, if app level notifications are disabled.
     * (2) For Android versions >= T, if we don't have notification permission and have exhausted
     * our permission request attempts.
     * @return Whether or not we are allowed to request site notification.
     */
    public abstract boolean doesAppLevelSettingsAllowSiteNotifications();
}

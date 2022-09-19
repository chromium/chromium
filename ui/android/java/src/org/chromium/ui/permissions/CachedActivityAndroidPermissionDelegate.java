// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.ui.base.UiBaseFeatureList;
import org.chromium.ui.base.UiBaseFeatures;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/**
 * AndroidPermissionDelegate implementation for Activity with caching.
 */
public class CachedActivityAndroidPermissionDelegate extends ActivityAndroidPermissionDelegate {
    private Map<String, Boolean> mHasPermissionCache;
    private Map<String, Boolean> mCanRequestPermissionCache;
    private Boolean mCacheEnabled;

    public CachedActivityAndroidPermissionDelegate(WeakReference<Activity> activity) {
        super(activity);

        mCanRequestPermissionCache = new HashMap<String, Boolean>();
        mHasPermissionCache = new HashMap<String, Boolean>();
    }

    @VisibleForTesting
    void setCacheEnabledForTesting(Boolean cacheEnabled) {
        mCacheEnabled = cacheEnabled;
    }

    /**
     * Clears all cached values.
     */
    public void invalidateCache() {
        if (cacheEnabled()) {
            mCanRequestPermissionCache.clear();
            mHasPermissionCache.clear();
        }
    }

    @Override
    public final boolean hasPermission(String permission) {
        if (cacheEnabled()) {
            Boolean cachedValue = mHasPermissionCache.get(permission);
            if (cachedValue != null) {
                return cachedValue;
            }
        }

        boolean hasPermission = super.hasPermission(permission);

        if (cacheEnabled()) {
            mHasPermissionCache.put(permission, hasPermission);
        }

        return hasPermission;
    }

    @Override
    public final boolean canRequestPermission(String permission) {
        if (cacheEnabled()) {
            Boolean cachedValue = mCanRequestPermissionCache.get(permission);
            if (cachedValue != null) {
                return cachedValue;
            }
        }

        boolean canRequestPermission = super.canRequestPermission(permission);

        if (cacheEnabled()) {
            mCanRequestPermissionCache.put(permission, canRequestPermission);
        }

        return canRequestPermission;
    }

    private boolean cacheEnabled() {
        if (mCacheEnabled == null) {
            mCacheEnabled = FeatureList.isNativeInitialized()
                    && UiBaseFeatureList.isEnabled(UiBaseFeatures.ANDROID_PERMISSIONS_CACHE);
        }

        return mCacheEnabled;
    }
}

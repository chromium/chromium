// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import android.app.Activity;

import java.lang.ref.WeakReference;

/** AndroidPermissionDelegate implementation for Activity. */
public class ActivityAndroidPermissionDelegate extends AndroidPermissionDelegateWithRequester {
    private WeakReference<Activity> mActivity;

    public ActivityAndroidPermissionDelegate(WeakReference<Activity> activity) {
        mActivity = activity;
    }

    @Override
    public final boolean shouldShowRequestPermissionRationale(String permission) {
        Activity activity = mActivity.get();
        if (activity == null) return false;
        return activity.shouldShowRequestPermissionRationale(permission);
    }

    @Override
    protected final boolean isPermissionRevokedByPolicyInternal(String permission) {
        Activity activity = mActivity.get();
        if (activity == null) return false;
        return activity.getPackageManager()
                .isPermissionRevokedByPolicy(permission, activity.getPackageName());
    }

    @Override
    protected boolean requestPermissionsFromRequester(String[] permissions, int requestCode) {
        Activity activity = mActivity.get();
        if (activity == null) return false;
        activity.requestPermissions(permissions, requestCode);
        return true;
    }
}

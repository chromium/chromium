// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;

import org.chromium.base.compat.ApiHelperForM;

import java.lang.ref.WeakReference;

/**
 * AndroidPermissionDelegate implementation for Activity.
 */
public class ActivityAndroidPermissionDelegate extends AndroidPermissionDelegateWithRequester {
    private WeakReference<Activity> mActivity;

    public ActivityAndroidPermissionDelegate(WeakReference<Activity> activity) {
        mActivity = activity;
    }

    @Override
    protected final boolean shouldShowRequestPermissionRationale(String permission) {
        Activity activity = mActivity.get();
        if (activity == null) return false;
        return ApiHelperForM.shouldShowRequestPermissionRationale(activity, permission);
    }

    @Override
    protected final boolean isPermissionRevokedByPolicyInternal(String permission) {
        Activity activity = mActivity.get();
        if (activity == null) return false;
        return ApiHelperForM.isPermissionRevokedByPolicy(activity, permission);
    }

    @Override
    protected final boolean requestPermissionsFromRequester(String[] permissions, int requestCode) {
        Activity activity = mActivity.get();
        if (activity == null) return false;
        ApiHelperForM.requestActivityPermissions(activity, permissions, requestCode);
        return true;
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.compat.ApiHelperForM;
import org.chromium.ui.base.AndroidPermissionDelegateWithRequester;

/**
 * AndroidPermissionDelegate implementation for BrowserFragment.
 */
public class FragmentAndroidPermissionDelegate extends AndroidPermissionDelegateWithRequester {
    private BrowserFragmentImpl mFragment;

    public FragmentAndroidPermissionDelegate(BrowserFragmentImpl fragment) {
        mFragment = fragment;
    }

    @Override
    protected final boolean shouldShowRequestPermissionRationale(String permission) {
        if (mFragment.getActivity() == null) return false;
        return mFragment.shouldShowRequestPermissionRationale(permission);
    }

    @Override
    protected final boolean isPermissionRevokedByPolicyInternal(String permission) {
        if (mFragment.getActivity() == null) return false;
        return ApiHelperForM.isPermissionRevokedByPolicy(mFragment.getActivity(), permission);
    }

    @Override
    protected final boolean requestPermissionsFromRequester(String[] permissions, int requestCode) {
        if (mFragment.getActivity() == null) return false;
        mFragment.requestPermissions(permissions, requestCode);
        return true;
    }
}

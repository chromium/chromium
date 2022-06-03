// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;

import androidx.fragment.app.Fragment;

import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 * The client-side implementation of MediaRouteDialogFragment.
 *
 * This class hosts dialog fragments for casting, such as a {@link MediaRouteChooserDialogFragment}
 * or a {@link MediaRouteControllerDialogFragment}.
 */
public class MediaRouteDialogFragment extends RemoteFragment {
    private static final String FRAGMENT_TAG = "WebLayerMediaRouteDialogFragment";

    static IRemoteFragment create(Fragment browserFragment) {
        MediaRouteDialogFragment fragment = new MediaRouteDialogFragment();
        browserFragment.getParentFragmentManager()
                .beginTransaction()
                .add(0, fragment, FRAGMENT_TAG)
                .commitNow();
        return fragment.getRemoteFragment();
    }

    @Override
    protected IRemoteFragment createRemoteFragment(Context appContext) {
        try {
            return WebLayer.loadSync(appContext)
                    .connectMediaRouteDialogFragment(getRemoteFragmentClient())
                    .asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;

import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 * The client-side implementation of a SettingsFragment.
 *
 * This is a Fragment that can be shown within an embedder's UI, and proxies its lifecycle events to
 * a SettingsFragmentImpl object on the implementation side. This class is an implementation detail
 * and should not be used by an embedder directly.
 *
 * @since 89
 */
public class SettingsFragment extends RemoteFragment {
    public SettingsFragment() {
        super();
    }

    @Override
    protected IRemoteFragment createRemoteFragment(Context appContext) {
        try {
            Bundle args = getArguments();
            if (args == null) {
                throw new RuntimeException("SettingsFragment was created without arguments.");
            }
            // TODO(crbug.com/1106393): This can be removed once M88 is no longer supported.
            if (WebLayer.getSupportedMajorVersionInternal() < 89) {
                return WebLayer.loadSync(appContext)
                        .connectSiteSettingsFragment(getRemoteFragmentClient(), args)
                        .asRemoteFragment();
            }
            return WebLayer.loadSync(appContext)
                    .connectSettingsFragment(getRemoteFragmentClient(), args)
                    .asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }
}

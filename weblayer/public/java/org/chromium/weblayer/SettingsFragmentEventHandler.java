// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;

import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 * This is for Fragments that can be shown within an embedder's UI, and proxies its lifecycle events
 * to a SettingsFragmentImpl object on the implementation side. This class is an implementation
 * detail and should not be used by an embedder directly.
 *
 * TODO(rayankans): Expose Settings to the client side.
 *
 * @since 89
 */
class SettingsFragmentEventHandler extends RemoteFragmentEventHandler {
    public SettingsFragmentEventHandler(Bundle args) {
        super(args);
        assert args != null;
    }

    @Override
    protected IRemoteFragment createRemoteFragmentEventHandler(Context appContext) {
        try {
            Bundle args = getArguments();
            return WebLayer.loadSync(appContext)
                    .connectSettingsFragment(getRemoteFragmentClient(), args)
                    .asRemoteFragment();
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }
}

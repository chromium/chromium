// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 * Class to handle events forwarded by WebFragmentEventDelegate.
 */
final class WebFragmentEventHandler extends RemoteFragmentEventHandler {
    public WebFragmentEventHandler(Browser browser) {
        super(browser);
    }

    @Override
    protected IRemoteFragment createRemoteFragmentEventHandler(Browser browser) {
        try {
            IBrowserFragment browserFragment = browser.connectFragment();
            return browserFragment.asRemoteFragment();
        } catch (RemoteException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }
}
